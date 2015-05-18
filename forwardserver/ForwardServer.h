#ifndef FORWARD_SERVER_H
#define FORWARD_SERVER_H

#include <proto/query.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>

namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::Query> QueryPtr;
typedef boost::shared_ptr<muduo::Answer> AnswerPtr;

class ForwardServer : boost::noncopyable
{
 public:
  ForwardServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr,
               muduo::Lua* l,
               uint16_t threadCount);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onQuery(const muduo::net::TcpConnectionPtr& conn,
               const QueryPtr& message,
               muduo::Timestamp);

  void onAnswer(const muduo::net::TcpConnectionPtr& conn,
                const AnswerPtr& message,
                muduo::Timestamp);

  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
};

#endif
