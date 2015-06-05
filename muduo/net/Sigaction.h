#ifndef SIGACTION_H
#define SIGACTION_H

namespace muduo
{
namespace net
{

class EventLoop;
extern EventLoop* mainEventLoop;

void setMainEventLoop(EventLoop* loop);

}
}

#endif
