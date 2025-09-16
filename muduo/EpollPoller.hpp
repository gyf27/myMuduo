#pragma once

#include <sys/epoll.h>
#include <vector>

#include "Poller.hpp"
#include "Timestamp.hpp"

class Channel;

class EpollPoller:public Poller{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    //重写基类的抽象方法
    Timestamp poll(int timeoutMs,ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    static const int kInitEventListSize=16;
    //填写活跃的连接
    void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;
    //更新channel通道
    void update(int operation,Channel* channel);
    using EventList=std::vector<struct epoll_event>;
    int epollfd_;
    EventList events_;
};