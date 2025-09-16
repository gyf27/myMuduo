#include "Socket.hpp"
#include "Logger.hpp"
#include "InetAddress.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

Socket::~Socket(){
	::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr){
	if(0!=::bind(sockfd_,(struct sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in))){
		LOG_FATAL("bind sockfd:%d fail \n",sockfd_);
	}
}

void Socket::listen(){
	if(0!=::listen(sockfd_,1024)){
		LOG_FATAL("listen sockfd:%d fail \n",sockfd_);
	}
}

int Socket::accept(InetAddress *peeraddr){
	sockaddr_in addr;
	socklen_t len=sizeof(addr);
	bzero(&addr,sizeof(addr));
	int connfd=::accept4(sockfd_,(struct sockaddr*)&addr,&len,SOCK_NONBLOCK|SOCK_CLOEXEC);
	if(connfd>=0){
		peeraddr->setSockAddr(addr);
	}
	return connfd;
}

void Socket::shutdownWrite(){
	if(::shutdown(sockfd_,SHUT_WR)<0){
		LOG_ERROR("shutdownWrite Error\n");
	}
}

void Socket::setTcpNoDelay(bool on){
	int opt=on?1:0;
	::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&opt,sizeof(opt));
}

void Socket::setReuseAddr(bool on){
	int opt=on?1:0;
	::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
}

void Socket::setReusePort(bool on){
	int opt=on?1:0;
	::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt));
}

void Socket::setKeepAlive(bool on){
	int opt=on?1:0;
	::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&opt,sizeof(opt));
}