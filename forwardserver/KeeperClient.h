#ifndef KEEPER_CLIENT_H
#define KEEPER_CLIENT_H

#include <proto/keeperForward.pb.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/TcpConnection.h>
typedef boost::shared_ptr<keeperForward::KeeperAnswer> KeeperAnswerPtr;
typedef boost::shared_ptr<keeperForward::KeeperForwardLogonSession> KeeperForwardLogonSessionPtr;

namespace muduo
{
class Lua;
}

class ForwardServer;

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

  void onKeeperForwardLogonSession(const muduo::net::TcpConnectionPtr&,
                                   const KeeperForwardLogonSessionPtr& message,
                                   muduo::Timestamp);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpClient client_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
  muduo::net::TcpConnectionPtr conn_;
  muduo::MutexLock mutex_;

  boost::scoped_ptr<ForwardServer> forwardServer_;
 public: 
  template<typename MSG>
void sendToKeeper(const MSG& msg)
{
  muduo::net::TcpConnectionPtr conn;
  {
    muduo::MutexLockGuard lock(mutex_);
    conn = conn_;
  }
  if (conn)
  {
    codec_.send(conn, msg);
  }
}

};


#endif
