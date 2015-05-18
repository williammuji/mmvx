#ifndef KEEPER_CLIENT_H
#define KEEPER_CLIENT_H

#include <proto/keeperQuery.pb.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>

typedef boost::shared_ptr<muduo::KeeperAnswer> KeeperAnswerPtr;

namespace muduo
{
class Lua;
}

class KeeperClient : boost::noncopyable
{
 public:
  KeeperClient(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& serverAddr,
               muduo::Lua* l);

  void connect();

 private:

  void onConnection(const muduo::net::TcpConnectionPtr& conn);
  void onUnknownMessage(const muduo::net::TcpConnectionPtr&,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onAnswer(const muduo::net::TcpConnectionPtr&,
                const KeeperAnswerPtr& message,
                muduo::Timestamp);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpClient client_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
};


#endif
