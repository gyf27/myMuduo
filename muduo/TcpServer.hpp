#pragma once 
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.hpp"
#include "Acceptor.hpp"
#include "InetAddress.hpp"
#include "noncopyable.hpp"
#include "EventLoopThreadPool.hpp"
#include "Callbacks.hpp"
#include "TcpConnection.hpp"
#include "Buffer.hpp"

class TcpServer:noncopyable{
public:
	using ThreadInitCallback=std::function<void(EventLoop*)>;
	enum Option{
		kNoReusePort,
		kReusePort,
	};
	TcpServer(EventLoop* loop,const InetAddress& listenAddr,const std::string& nameArg,Option option=kNoReusePort);
	~TcpServer();

	void setThreadInitCallback(const ThreadInitCallback& cb){threadInitCallback_=cb;}
	void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_=cb;}
	void setMessageCallback(const MessageCallback& cb){messageCallback_=cb;}
	void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_=cb;}

	//设置subloop的个数
	void setThreadNum(int num);

	//开启服务器监听
	void start();

private:
	void newConnection(int sockfd,const InetAddress& peerAddr);
	void removeConnection(const TcpConnectionPtr& conn);
	void removeConnectionInLoop(const TcpConnectionPtr& conn);

	using ConnectionMap=std::unordered_map<std::string,TcpConnectionPtr>;

	EventLoop* loop_;  //即baseloop

	const std::string ipPort_;
	const std::string name_;

	std::unique_ptr<Acceptor> acceptor_;	//运行在mainloop，任务就是监听新事件的连接
	std::shared_ptr<EventLoopThreadPool> threadPool_;	//one loop per thread指向线程池的智能指针

	ConnectionCallback connectionCallback_;	//有新连接时的回调
	MessageCallback messageCallback_;	//有读写消息的回调
	WriteCompleteCallback writeCompleteCallback_; 	//消息发送完成以后的回调

	ThreadInitCallback threadInitCallback_;	//线程初始化时候的回调

	std::atomic_int started_;

	int nextConnId_;
	ConnectionMap connections_;	//	保存所有连接

};