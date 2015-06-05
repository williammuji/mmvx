#ifndef KEEPER_SERVER_H
#define KEEPER_SERVER_H

#include <proto/keeperQuery.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::KeeperQuery> KeeperQueryPtr;
typedef boost::shared_ptr<muduo::KeeperAnswer> KeeperAnswerPtr;

class KeeperServer : boost::noncopyable
{
 public:
  KeeperServer(muduo::net::EventLoop* loop,
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
               const KeeperQueryPtr& message,
               muduo::Timestamp);

  void onAnswer(const muduo::net::TcpConnectionPtr& conn,
                const KeeperAnswerPtr& message,
                muduo::Timestamp);

  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;

  struct ServerConfig
  {
    uint16_t id;
    uint16_t ttype;
    std::string ip;
    uint16_t port;
    uint16_t threads;
  };
  std::vector<ServerConfig> serverConfig_;
  void loadServerConfig();

  struct ServerStart
  {
    ServerStart(uint16_t i, bool s) : id(i),start(s){}
    uint16_t id;
    bool start;
  };
  std::vector<ServerStart> serverStart_;
  bool isStarted(uint16_t serverID);
  void setStarted(uint16_t serverID);
};

#endif
