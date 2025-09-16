#include <mymuduo/TcpServer.hpp>
#include <mymuduo/Logger.hpp>
#include <string>


class EchoServer{
public:
	EchoServer(EventLoop* loop,const InetAddress& addr,const std::string& name)
		:server_(loop,addr,name)
		,loop_(loop)
	{
		//注册回调函数
		
		//设置合适的loop线程数
	}
private:
	//建立或断开连接的回调
	void onConnection(const TcpConnectionPtr& conn){
		if(conn->connected()){
			LOG_INFO("conn UP: %s",conn->peerAddress().toIpPort().c_str());
		}
		else{
			LOG_INFO("conn DOWN: %s",conn->peerAddress().toIpPort().c_str());
		}
	}

	//可读写事件回调
	void onMessage(const TcpConnectionPtr& conn,Buffer* buf,Timestamp time){
		std::string msg=buf->retrieveAllAsString();
		conn->send(msg);
	}

	EventLoop* loop_;
	TcpServer server_;
};

int main(){

	return 0;
}