#include "KeeperClient.h"
#include <muduo/net/ServerType.h>
#include "ForwardServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
#include <stdio.h>
using namespace muduo;
using namespace muduo::net;
using namespace keeperForward;

KeeperClient::KeeperClient(EventLoop* loop,
                           const InetAddress& serverAddr,
                           Lua* l)
  : loop_(loop),
    client_(loop, serverAddr, "KeeperClient"),
    dispatcher_(boost::bind(&KeeperClient::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l)
{
  assert(lua_);
  dispatcher_.registerMessageCallback<KeeperAnswer>(
      boost::bind(&KeeperClient::onAnswer, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<KeeperForwardLogonSession>(
      boost::bind(&KeeperClient::onKeeperForwardLogonSession, this, _1, _2, _3));
  client_.setConnectionCallback(
      boost::bind(&KeeperClient::onConnection, this, _1));
  client_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  client_.enableRetry();

  srand(time(NULL));
}

void KeeperClient::connect()
{
  client_.connect();
}

void KeeperClient::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    MutexLockGuard lock(mutex_);
    conn_ = conn;

    KeeperQuery query;
    query.set_servertype(SERVER_TYPE_FORWARD);
    codec_.send(conn, query);
  }
  else
  {
    MutexLockGuard lock(mutex_);
    conn_.reset();
  }
}

void KeeperClient::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();

  conn->shutdown();
}

void KeeperClient::onAnswer(const muduo::net::TcpConnectionPtr&,
              const KeeperAnswerPtr& message,
              muduo::Timestamp)
{
  LOG_INFO << "onAnswer:\n" << message->GetTypeName() << message->DebugString();

  InetAddress listenAddr(message->ip(), message->port());
  forwardServer_.reset(new ForwardServer(loop_, listenAddr, lua_, message->threads(), message->keeperserverid(), message->serverid(), this));
  forwardServer_->start();
}

void KeeperClient::onKeeperForwardLogonSession(const muduo::net::TcpConnectionPtr&,
                                               const KeeperForwardLogonSessionPtr& message,
                                               muduo::Timestamp)
{
  LOG_INFO << "onKeeperForwardLogonSession:\n" << message->GetTypeName() << message->DebugString();

  assert(forwardServer_);

  int32_t session = rand();
  forwardServer_->addLogonSession(message->uid(), session);

  KeeperForwardLogonSessionRet send;
  send.set_uid(message->uid());
  send.set_connname(message->connname());
  send.set_forwardip(forwardServer_->listenAddress().toIp().data());
  send.set_forwardport(forwardServer_->listenAddress().toPort());
  send.set_session(session);
  sendToKeeper(send);
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("forwardserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  lua_tinker::table keeperConfig = luaMgr.call<lua_tinker::table>("getKeeperServerConfig");
  const char* ip = keeperConfig.get<const char*>("ip");
  uint16_t port = keeperConfig.get<uint16_t>("port");

  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  InetAddress serverAddr(ip, port);

  KeeperClient client(&loop, serverAddr, &luaMgr);
  client.connect();
  loop.loop();
}

