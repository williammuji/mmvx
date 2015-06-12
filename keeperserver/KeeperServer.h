#ifndef KEEPER_SERVER_H
#define KEEPER_SERVER_H

#include <proto/keeperForward.pb.h>
#include <proto/hubKeeper.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpConnection.h>
namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<keeperForward::KeeperQuery> KeeperQueryPtr;
typedef boost::shared_ptr<keeperForward::KeeperForwardLogonSessionRet> KeeperForwardLogonSessionRetPtr;
typedef boost::shared_ptr<keeperForward::KeeperForwardUpdateForwardConns> KeeperForwardUpdateForwardConnsPtr;
typedef boost::shared_ptr<muduo::HubKeeperLogonSession> HubKeeperLogonSessionPtr;

class KeeperServer : boost::noncopyable
{
 public:
  KeeperServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr,
               const muduo::net::InetAddress& hubAddr,
               muduo::Lua* l,
               uint16_t threadCount,
               uint16_t serverID);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onQuery(const muduo::net::TcpConnectionPtr& conn,
               const KeeperQueryPtr& message,
               muduo::Timestamp);

  void onKeeperForwardLogonSessionRet(const muduo::net::TcpConnectionPtr& conn,
                                      const KeeperForwardLogonSessionRetPtr& message,
                                      muduo::Timestamp);

  void onKeeperForwardUpdateForwardConns(const muduo::net::TcpConnectionPtr& conn,
                                         const KeeperForwardUpdateForwardConnsPtr& message,
                                         muduo::Timestamp);

  void onHubKeeperLogonSession(const muduo::net::TcpConnectionPtr& conn,
                               const HubKeeperLogonSessionPtr& message,
                               muduo::Timestamp);


  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
  const uint16_t serverID_;
  typedef uint16_t ForwardServerID;
  typedef std::map<ForwardServerID, muduo::net::TcpConnectionPtr> ConnectionMap;
  ConnectionMap conns_;
  muduo::MutexLock mutex_;

  muduo::net::TcpClient hubClient_;
  muduo::net::ProtobufDispatcher hubDispatcher_;
  muduo::net::ProtobufCodec hubCodec_;
  muduo::net::TcpConnectionPtr hubConn_;
  muduo::MutexLock hubMutex_;

  void onHubConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownHubMessage(const muduo::net::TcpConnectionPtr&,
                           const muduo::net::MessagePtr& message,
                           muduo::Timestamp);


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
    ServerStart(uint16_t i, bool s, const muduo::string& n) : id(i),start(s),connName(n){}
    uint16_t id;
    bool start;
    muduo::string connName;
  };
  std::vector<ServerStart> serverStart_;
  bool isStarted(uint16_t serverID);
  void setStarted(uint16_t serverID, bool start, const muduo::net::TcpConnectionPtr& conn);

  template<typename MSG>
void sendToHub(const MSG& msg)
{
  muduo::net::TcpConnectionPtr hubConn;
  {
    muduo::MutexLockGuard lock(hubMutex_);
    hubConn = hubConn_;
  }
  if (hubConn)
  {
    hubCodec_.send(hubConn, msg);
  }
}

template<typename MSG>
void sendToForward(ForwardServerID id, const MSG& msg)
{
  muduo::net::TcpConnectionPtr conn;
  {
    muduo::MutexLockGuard lock(mutex_);
    ConnectionMap::iterator it = conns_.find(id);
    if (it != conns_.end())
    {
      conn = it->second;
    }
  }
  if (conn)
  {
    codec_.send(conn, msg);
  }
}

};

#endif
