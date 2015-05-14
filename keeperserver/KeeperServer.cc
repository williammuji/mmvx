#include "KeeperServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

using namespace muduo;
using namespace muduo::net;


KeeperServer::KeeperServer(EventLoop* loop,
                           const InetAddress& listenAddr,
                           Lua* l)
  : server_(loop, listenAddr, "KeeperServer"),
    dispatcher_(boost::bind(&KeeperServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l)
{
  dispatcher_.registerMessageCallback<muduo::Query>(
      boost::bind(&KeeperServer::onQuery, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::Answer>(
      boost::bind(&KeeperServer::onAnswer, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&KeeperServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
}

void KeeperServer::start()
{
  server_.start();
}

void KeeperServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");
}

void KeeperServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void KeeperServer::onQuery(const muduo::net::TcpConnectionPtr& conn,
                           const QueryPtr& message,
                           muduo::Timestamp)
{
  LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
  Answer answer;
  answer.set_id(1);
  answer.set_questioner("Chen Shuo");
  answer.set_answerer("blog.csdn.net/Solstice");
  answer.add_solution("Jump!");
  answer.add_solution("Win!");
  codec_.send(conn, answer);

  conn->shutdown();
}

void KeeperServer::onAnswer(const muduo::net::TcpConnectionPtr& conn,
                            const AnswerPtr& message,
                            muduo::Timestamp)
{
  LOG_INFO << "onAnswer: " << message->GetTypeName();
  conn->shutdown();
}

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  Logger::setLogLevel(Logger::TRACE);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  lua_tinker::table keeperConfig = luaMgr.call<lua_tinker::table>("getKeeperServerConfig");
  const char* ip = keeperConfig.get<const char*>("ip");
  uint16_t port = keeperConfig.get<uint16_t>("port");

  InetAddress listenAddr(ip, port);
  EventLoop loop;
  KeeperServer server(&loop, listenAddr, &luaMgr);

  server.start();
  loop.loop();
}

