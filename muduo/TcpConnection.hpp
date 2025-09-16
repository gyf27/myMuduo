#pragma once 
#include <memory>
#include <string>
#include <atomic>

#include "Buffer.hpp"
#include "noncopyable.hpp"
#include "InetAddress.hpp"
#include "Callbacks.hpp"
#include "Timestamp.hpp"

class Channel;
class EventLoop;
class Socket;

/*
TcpServer通过Acceptor监听到一个新用户连接时，通过accept()函数拿到connfd
然后打包TcpConnection（设置相应回调）=》发送给Channel=》注册到Poller上，Poller监听到事件发生就会执行Channel的回调操作
*/

class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>{
public:
	TcpConnection(EventLoop* loop
				,const std::string &name
				,int sockfd
				,const InetAddress& localAddr
				,const InetAddress& peerAddr);
	~TcpConnection();
	EventLoop* getLoop()const{return loop_;}
	const std::string& name()const{return name_;}
	const InetAddress& localAddress()const{return localAddr_;}
	const InetAddress& peerAddress()const{return peerAddr_;}

	bool connected()const {return state_==kConnected;}

	void send(const std::string& buf);
	//void send(const void *message,int len);
	void shutdown();

	void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_=cb;}
	void setMessageCallback(const MessageCallback& cb){messageCallback_=cb;}
	void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_=cb;}
	void setCloseCallback(const CloseCallback& cb){closeCallback_=cb;}
	void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,size_t highWaterMark)
	{
		highWaterMarkCallback_=cb;
		highWaterMark_=highWaterMark;
	}

	//连接建立
	void connectEstablished();
	//连接销毁
	void connectDestoryed();
	
private:
	enum StateE{kDisconnected,kConnecting,kConnected,kDisconnecting};
	void setState(StateE state){state_=state;}
	
	void handleRead(Timestamp receiveTime);
	void handleWrite();
	void handleClose();
	void handleError();

	
	void sendInLoop(const void* data,size_t len);
	void shutdownInLoop();

	EventLoop* loop_;  //这里绝对不是baseloop，因为TcpConnection都是在subloop里管理的
	const std::string name_;
	std::atomic_int state_;
	bool reading_;

	std::unique_ptr<Socket> socket_;
	std::unique_ptr<Channel> channel_;

	const InetAddress localAddr_;
	const InetAddress peerAddr_;

	ConnectionCallback connectionCallback_;	//有新连接时的回调
	MessageCallback messageCallback_;	//有读写消息的回调
	WriteCompleteCallback writeCompleteCallback_; 	//消息发送完成以后的回调
	HighWaterMarkCallback highWaterMarkCallback_;
	CloseCallback closeCallback_;
	size_t highWaterMark_;

	Buffer inputBuffer_;
	Buffer outputBuffer_;

};