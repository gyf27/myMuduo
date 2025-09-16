#include "TcpServer.hpp"
#include "Logger.hpp"

#include <functional>
#include <strings.h>

static EventLoop* CheckNotNull(EventLoop* loop){
	if(loop==nullptr){
		LOG_FATAL("%s:%s:%d main loop is null \n",__FILE__,__FUNCTION__,__LINE__);
	}
	return loop;
}

//TCPServer构造的作用：创建一个socket(lfd)，将这个socket分装成channel，将这个channel添加到
//当前loop的poller里，并设置回调
TcpServer::TcpServer(EventLoop* loop,const InetAddress& listenAddr,const std::string& nameArg,Option option)
	:loop_(CheckNotNull(loop))
	,ipPort_(listenAddr.toIpPort())
	,name_(nameArg)
	,acceptor_(new Acceptor(loop,listenAddr,option==kReusePort))
	,threadPool_(new EventLoopThreadPool(loop,name_))
	,connectionCallback_()
	,messageCallback_()
	,nextConnId_(1)
	,started_(0)
{
	//当有用户连接，会执行TcpServer::newConnection回调
	acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
		std::placeholders::_1,std::placeholders::_2));

}

TcpServer::~TcpServer(){
	for(auto& item:connections_){
		TcpConnectionPtr conn(item.second);
		item.second.reset();
		//销毁连接
		conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestoryed,conn));
	}
}

void TcpServer::setThreadNum(int num){
	threadPool_->setThreadNum(num);
}

//开启服务器监听
void TcpServer::start(){
	if(started_++==0){	//防止一个TcpServer对象被多次start
		threadPool_->start(threadInitCallback_); 	//启动底层loop线程池
		loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
	}
}

void TcpServer::newConnection(int sockfd,const InetAddress& peerAddr){
	//轮询算法选择一个subloop来管理channel
	EventLoop* ioLoop=threadPool_->getNextLoop();
	char buf[64]={0};
	snprintf(buf,sizeof(buf),"-%s#%d",ipPort_.c_str(),nextConnId_);
	++nextConnId_;
	std::string connName=name_+buf;

	LOG_INFO("TcpServer::newConnection [%s]=new connection [%s] from %s\n",
		name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());
	
	struct sockaddr_in local;
	::bzero(&local,sizeof(local));
	socklen_t addrlen=sizeof(local);
	if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0){
		LOG_ERROR("sockets::getlocalAddr\n");
	}
	InetAddress localAddr(local);

	//根据连接成功的sockfd创建TcpConnection连接对象
	TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));

	connections_[connName]=conn;
	//下面的回调都是用户设置给TcpServer=》TcpConnection=》Channel=》Poller最后通知channel调用回调
	conn->setConnectionCallback(connectionCallback_);
	conn->setMessageCallback(messageCallback_);
	conn->setWriteCompleteCallback(writeCompleteCallback_);

	conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));

	//直接调用TcpConnection::connectEstablished
	ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}


void TcpServer::removeConnection(const TcpConnectionPtr& conn){
	loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));

}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
	LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",name_.c_str(),conn->name().c_str());
	connections_.erase(conn->name());
	EventLoop* ioLoop=conn->getLoop();
	ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestoryed,conn));

}
