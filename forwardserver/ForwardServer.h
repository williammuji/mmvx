#ifndef FORWARD_SERVER_H
#define FORWARD_SERVER_H

#include <proto/logon.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>

namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::LogonForward> LogonForwardPtr;

class KeeperClient;

class ForwardServer : boost::noncopyable
{
 public:
  ForwardServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr,
               muduo::Lua* l,
               uint16_t threadCount,
               uint16_t keeperServerID,
               uint16_t serverID,
               KeeperClient* client);

  void start();

  void addLogonSession(int64_t uid, int32_t session);

  const muduo::net::InetAddress& listenAddress() const { return listenAddr_; }

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onLogonForward(const muduo::net::TcpConnectionPtr& conn,
                      const LogonForwardPtr& message,
                      muduo::Timestamp);
  void checkLogonForwardSession(const muduo::net::TcpConnectionPtr& conn,
                                const LogonForwardPtr& message,
                                muduo::Timestamp);


  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  const muduo::net::InetAddress listenAddr_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
  const uint16_t keeperServerID_;
  const uint16_t serverID_;
  KeeperClient* client_;

  typedef std::map<int64_t, int32_t> LogonSessionsMap;
  LogonSessionsMap logonSessions_;
};

#endif
