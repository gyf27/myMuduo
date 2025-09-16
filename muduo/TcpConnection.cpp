#include "TcpConnection.hpp"
#include "Logger.hpp"
#include "Socket.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <functional>
#include <errno.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
	if (loop == nullptr)
	{
		LOG_FATAL("%s:%s:%d TcpConnection loop is null \n", __FILE__, __FUNCTION__, __LINE__);
	}
	return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
	: loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting), reading_(true), socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024)
{
	// 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生以后，channel会执行相应的回调
	channel_->setReadCallback(
		std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
	channel_->setWriteCallback(
		std::bind(&TcpConnection::handleWrite, this));
	channel_->setCloseCallback(
		std::bind(&TcpConnection::handleClose, this));
	channel_->setERRORCallback(
		std::bind(&TcpConnection::handleError, this));
	LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
	socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
	LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string& buf){
	if(state_==kConnected){
		if(loop_->isInLoopThread()){
			sendInLoop(buf.c_str(),buf.size());
		}
		else{
			loop_->runInLoop(std::bind(
				&TcpConnection::sendInLoop,this,buf.c_str(),buf.size()));
		}
	}
}

void TcpConnection::sendInLoop(const void* data,size_t len){
	ssize_t nwrote=0;
	size_t remaining=len;
	bool faultError=false;
	//之前调用过connection的shutdown，不能再进行发送了
	if(state_==kDisconnected){
		LOG_ERROR("disconnected ,give up writing!\n");
		return;
	}
	//表示channel第一次开始写数据而且缓冲区没有待发送数据
	if(!channel_->isWriting()&&outputBuffer_.readableBytes()==0){
		nwrote=::write(channel_->fd(),data,len);
		if(nwrote>=0){
			remaining=len-nwrote;
			if(remaining==0&&writeCompleteCallback_){
				loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
			}
		}
		else{
			nwrote=0;
			if(errno!=EWOULDBLOCK){
				LOG_ERROR("TcpConnection::sendInLoop\n");
				if(errno==EPIPE||errno==ECONNRESET){
					faultError=true;
				}
			}
		}
	}
	//说明这一次write没有把数据全部发送出去，剩余的数据需要保存到缓冲区，然后给channel注册epollout事件
	//poller发现tcp的发送缓冲区会通知相应的channel调用handle回调方法
	//也就是调用TcpConnection::handleWrite方法，把发送缓冲区的数据全部发送完成
	if(!faultError&&remaining>0){
		//目前发送缓冲区剩余的待发送数据的长度
		size_t oldLen=outputBuffer_.readableBytes();
		if(oldLen+remaining>=highWaterMark_&&oldLen<highWaterMark_&&highWaterMarkCallback_){
			loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));
		}
		outputBuffer_.append((char*)data+nwrote,remaining);
		if(!channel_->isWriting()){
			channel_->enableWriting();  //一定要注册channel的写事件
		}
	}
}

void TcpConnection::shutdown(){
	if(state_==kConnected){
		setState(kDisconnecting);
		loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
	}
}

void TcpConnection::shutdownInLoop(){
	if(!channel_->isWriting()){	//说明outputBuffer中的数据已经全部发送完成
		socket_->shutdownWrite();
	}
}

//连接建立
void TcpConnection::connectEstablished(){
	setState(kConnected);
	channel_->tie(shared_from_this());
	channel_->enableReading();

	connectionCallback_(shared_from_this());
}

//连接销毁
void TcpConnection::connectDestoryed(){
	if(state_==kConnected){
		setState(kDisconnected);
		channel_->disableAll();  //把channel所有感兴趣的事件，从poller中删除
		connectionCallback_(shared_from_this());
	}
	channel_->remove();//把channel从poller删除
}

void TcpConnection::handleRead(Timestamp receiveTime){
	int saveErrno=0;
	ssize_t n=inputBuffer_.readFd(channel_->fd(),&saveErrno);
	if(n>0){
		messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
	}
	else if(n==0){
		handleClose();
	}
	else{
		errno=saveErrno;
		LOG_ERROR("TcpConnection::handleRead\n");
		handleError();
	}
}

void TcpConnection::handleWrite(){
	if(channel_->isWriting()){
		int savedErrno=0;
		ssize_t n=outputBuffer_.writeFd(channel_->fd(),&savedErrno);
		if(n>0){
			outputBuffer_.retrieve(n);
			if(outputBuffer_.readableBytes()==0){
				channel_->disableWriting();
				if(writeCompleteCallback_){
					loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
				}
				if(state_==kDisconnecting){
					shutdownInLoop();
				}
			}
		}
		else{
			LOG_ERROR("TcpConnection::handleWrite\n");
		}
	}
	else{
		LOG_ERROR("TcpConnection fd=%d is down , no more writing \n",channel_->fd());
	}
}

void TcpConnection::handleClose(){
	LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
	setState(kDisconnected);
	channel_->disableAll();

	TcpConnectionPtr connPtr(shared_from_this());
	connectionCallback_(connPtr);
	closeCallback_(connPtr);
}

void TcpConnection::handleError(){
	int optval;
	socklen_t optlen=sizeof(optval);
	int err=0;
	if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0){
		err=errno;
	}
	else{
		err=optval;
	}
	LOG_ERROR("TcpConnection::handleError name=%s -SO_ERROR=%d \n",name_.c_str(),err);
}