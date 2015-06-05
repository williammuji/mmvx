#include "KeeperClient.h"
#include <muduo/net/ServerType.h>
#include "ForwardServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
using namespace muduo;
using namespace muduo::net;

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
  dispatcher_.registerMessageCallback<muduo::KeeperAnswer>(
      boost::bind(&KeeperClient::onAnswer, this, _1, _2, _3));
  client_.setConnectionCallback(
      boost::bind(&KeeperClient::onConnection, this, _1));
  client_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  client_.enableRetry();
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
    muduo::KeeperQuery query;
    query.set_servertype(SERVER_TYPE_FORWARD);
    codec_.send(conn, query);
  }
  else
  {
    //loop_->quit();
  }
}

void KeeperClient::onUnknownMessage(const TcpConnectionPtr&,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
}

void KeeperClient::onAnswer(const muduo::net::TcpConnectionPtr&,
              const KeeperAnswerPtr& message,
              muduo::Timestamp)
{
  LOG_INFO << "onAnswer:\n" << message->GetTypeName() << message->DebugString();

  InetAddress listenAddr(message->ip(), message->port());
  ForwardServer server(loop_, listenAddr, lua_, message->threads());
  server.start();
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

