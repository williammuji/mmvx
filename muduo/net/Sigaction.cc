#include <muduo/net/Sigaction.h>
#include <muduo/net/EventLoop.h>
#include <signal.h>
#include <muduo/base/Logging.h>

namespace muduo
{

namespace net
{
EventLoop* mainEventLoop = NULL;

void setMainEventLoop(EventLoop* loop)
{
  mainEventLoop = loop;
}
}


namespace detail
{
void stopServerHandler(int signum)
{
  assert(muduo::net::mainEventLoop != NULL);
  LOG_INFO << "stopServerHandler";
  muduo::net::mainEventLoop->quit();
  LOG_INFO << "stopServerHandler";
}

class SigactionInit
{
 public:
  SigactionInit()
  {
    struct sigaction sig;
    sig.sa_handler = stopServerHandler;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = 0;
    sigaction(SIGINT, &sig, NULL);
    sigaction(SIGQUIT, &sig, NULL);
    sigaction(SIGABRT, &sig, NULL);
    sigaction(SIGTERM, &sig, NULL);
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);
  }
};
SigactionInit siginit;
}


}
