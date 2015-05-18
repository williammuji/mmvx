#include "ForwardServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

using namespace muduo;
using namespace muduo::net;


ForwardServer::ForwardServer(EventLoop* loop,
                           const InetAddress& listenAddr,
                           Lua* l,
                           uint16_t threadCount)
  : server_(loop, listenAddr, "ForwardServer"),
    dispatcher_(boost::bind(&ForwardServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l)
{
  dispatcher_.registerMessageCallback<muduo::Query>(
      boost::bind(&ForwardServer::onQuery, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::Answer>(
      boost::bind(&ForwardServer::onAnswer, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&ForwardServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);
}

void ForwardServer::start()
{
  server_.start();
}

void ForwardServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");
}

void ForwardServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void ForwardServer::onQuery(const muduo::net::TcpConnectionPtr& conn,
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

void ForwardServer::onAnswer(const muduo::net::TcpConnectionPtr& conn,
                            const AnswerPtr& message,
                            muduo::Timestamp)
{
  LOG_INFO << "onAnswer: " << message->GetTypeName();
  conn->shutdown();
}
