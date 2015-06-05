#include "LogonServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/LogFile.h>
#include <muduo/net/ServerType.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
using namespace muduo;
using namespace muduo::net;

LogonServer::LogonServer(EventLoop* loop,
                         const InetAddress& listenAddr,
                         const InetAddress& hubAddr,
                         Lua* l,
                         uint16_t threadCount)
  : server_(loop, listenAddr, "LogonServer"),
    dispatcher_(boost::bind(&LogonServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l),
    hubClient_(loop, hubAddr, "HubClient"),
    hubDispatcher_(boost::bind(&LogonServer::onHubClientUnknownMessage, this, _1, _2, _3)),
    hubCodec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
{
  assert(lua_);

  dispatcher_.registerMessageCallback<muduo::Logon>(
      boost::bind(&LogonServer::onLogon, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&LogonServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);

  hubDispatcher_.registerMessageCallback<muduo::HubLogonAnswerForwardID>(
      boost::bind(&LogonServer::onHubClientLogonAnswerForwardID, this, _1, _2, _3));
  hubClient_.setConnectionCallback(
      boost::bind(&LogonServer::onHubClientConnection, this, _1));
  hubClient_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &hubCodec_, _1, _2, _3));
  hubClient_.enableRetry();
}

void LogonServer::start()
{
  hubClient_.connect();
  server_.start();
}

void LogonServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  MutexLockGuard lock(mutex_);
  if (conn->connected())
  {
    clientConns_[conn->name()] = conn;
  }
  else
  {
    clientConns_.erase(conn->name());
  }
}

void LogonServer::onHubClientConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  MutexLockGuard lock(hubMutex_);
  if (conn->connected())
  {
    hubConn_ = conn;
  }
  else
  {
    hubConn_.reset();
  }
}

void LogonServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                   const MessagePtr& message,
                                   Timestamp)
{
  LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void LogonServer::onHubClientUnknownMessage(const TcpConnectionPtr& conn,
                                            const MessagePtr& message,
                                            Timestamp)
{
  LOG_INFO << "onHubClientUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void LogonServer::onLogon(const muduo::net::TcpConnectionPtr& conn,
                          const LogonPtr& message,
                          muduo::Timestamp)
{
  LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  //TODO id name passwd verify 
  if (true)
  {
    HubLogonQueryForwardID query;
    query.set_uid(message->uid());
    query.set_connname(conn->name().c_str());
    query.set_keeperserverid(message->keeperserverid());
    sendToHub(query);
  }
  else
  {
    LogonRet ret;
    ret.set_uid(message->uid());
    ret.set_status("FAILED");
    codec_.send(conn, ret);
    conn->shutdown();
  }
}

void LogonServer::onHubClientLogonAnswerForwardID(const muduo::net::TcpConnectionPtr& conn,
                                                  const HubLogonAnswerForwardIDPtr& message,
                                                  muduo::Timestamp)
{
  LOG_INFO << "onAnswer:\n" << message->GetTypeName() << message->DebugString();

  //TODO send to client forward IP&port
  LogonRet ret;
  ret.set_uid(message->uid());
  if (message->has_forwardip())
  {
    ret.set_forwardip(message->forwardip());
    ret.set_status("SUCCESS");
  }
  if (message->has_forwardport())
    ret.set_forwardport(message->forwardport());

  sendToClient(message->connname().c_str(), ret);
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("logonserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");

  lua_tinker::table logonConfig = luaMgr.call<lua_tinker::table>("getLogonServerConfig");
  const char* ip = logonConfig.get<const char*>("ip");
  uint16_t port = logonConfig.get<uint16_t>("port");
  uint16_t threadCount = logonConfig.get<uint16_t>("threads");
  InetAddress listenAddr(ip, port);

  lua_tinker::table hubConfig = luaMgr.call<lua_tinker::table>("getHubServerIpPort");
  const char* hubIP = hubConfig.get<const char*>("ip");
  uint16_t hubPort = hubConfig.get<uint16_t>("port");
  InetAddress hubAddr(hubIP, hubPort);

  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  LogonServer server(&loop, listenAddr, hubAddr, &luaMgr, threadCount);

  server.start();
  loop.loop();
}

