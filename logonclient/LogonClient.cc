#include "LogonClient.h"
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
#include "ForwardClient.h"
using namespace muduo;
using namespace muduo::net;

LogonClient::LogonClient(muduo::net::EventLoop* loop,
                         const muduo::net::InetAddress& serverAddr,
                         const string& name,
                         LogonClientPool* pool)
: client_(loop, serverAddr, name),
    dispatcher_(boost::bind(&LogonClient::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    pool_(pool)
{
<<<<<<< HEAD
  codec_.setAes();

=======
>>>>>>> b318cba5298df589a32ab01efdb3f54d2d4e86fc
  dispatcher_.registerMessageCallback<muduo::LogonRet>(
      boost::bind(&LogonClient::onLogonRet, this, _1, _2, _3));
  client_.setConnectionCallback(
      boost::bind(&LogonClient::onConnection, this, _1));
  client_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

  //enable retry
}

void LogonClient::start()
{
  client_.connect();
}

void LogonClient::stop()
{
  client_.disconnect();
}

void LogonClient::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    conn->setTcpNoDelay(true);

    muduo::Logon query;
    query.set_uid(1);
    query.set_uname("1");
    query.set_passwd("123456");
    query.set_keeperserverid(10101);
    codec_.send(conn, query);
  }
  else
  {
  }
}

void LogonClient::onUnknownMessage(const TcpConnectionPtr& conn,
                                   const MessagePtr& message,
                                   Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();

  conn->shutdown();
}

void LogonClient::onLogonRet(const muduo::net::TcpConnectionPtr& conn,
                             const LogonRetPtr& message,
                             muduo::Timestamp)
{
  LOG_INFO << "onLogonRet:\n" << message->GetTypeName() << message->DebugString();

  if (message->status() == "FAILED")
    conn->shutdown();
  else if (message->status() == "SUCCESS")
  {
    InetAddress forwardAddr(message->forwardip(), message->forwardport());
    string forwardClientName(client_.name());
    forwardClientName.replace(0, 10, "ForwardClient");
    forwardClient_.reset(new ForwardClient(client_.getLoop(), forwardAddr, forwardClientName, message->uid(), message->session()));
    forwardClient_->start();
  }
}




LogonClientPool::LogonClientPool(muduo::net::EventLoop* loop,
                                 const muduo::net::InetAddress& serverAddr,
                                 int numClients,
                                 int numThreads)
  : loop_(loop),
    threadPool_(loop, "LogonClient"),
    numClients_(numClients)
{
  threadPool_.setThreadNum(numThreads);
  threadPool_.start();

  for (int i=0; i<numClients; ++i)
  {
    char buf[32] = {0};
    snprintf(buf, sizeof buf, "LogonClient%05d", i);
    LogonClient* logonClient = new LogonClient(threadPool_.getNextLoop(), serverAddr, buf, this);
    logonClient->start();
    logonClients_.push_back(logonClient);
  }
}

void LogonClientPool::quit()
{
  loop_->queueInLoop(boost::bind(&EventLoop::quit, loop_));
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("logonclient", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");

  lua_tinker::table logonConfig = luaMgr.call<lua_tinker::table>("getLogonServerConfig");
  const char* ip = logonConfig.get<const char*>("ip");
  uint16_t port = logonConfig.get<uint16_t>("port");
  uint16_t numClients = luaMgr.get<uint16_t>("LOGON_CLIENTS");
  uint16_t numThreads = luaMgr.get<uint16_t>("LOGON_CLIENT_THREADS");

  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  InetAddress serverAddr(ip, port);
  LogonClientPool clientPool(&loop, serverAddr, numClients, numThreads);
  loop.loop();
}

