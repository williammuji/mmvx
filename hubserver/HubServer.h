#ifndef HUB_SERVER_H
#define HUB_SERVER_H

#include <proto/hub.pb.h>
#include <proto/hubKeeper.pb.h>
#include <proto/hubLogon.pb.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <list>
#include <muduo/base/Mutex.h>
namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::HubKeeperUpdateForwardConns> HubKeeperUpdateForwardConnsPtr;
typedef boost::shared_ptr<muduo::HubLogonQueryForwardID> HubLogonQueryForwardIDPtr;
typedef boost::shared_ptr<muduo::HubConnectionData> HubConnectionDataPtr;
typedef boost::shared_ptr<muduo::HubKeeperLogonSessionRet> HubKeeperLogonSessionRetPtr;

class HubServer : boost::noncopyable
{
 public:
  HubServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listenAddr,
            muduo::Lua* l,
            uint16_t threadCount);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onHubKeeperUpdateForwardConns(const muduo::net::TcpConnectionPtr& conn,
               const HubKeeperUpdateForwardConnsPtr& message,
               muduo::Timestamp);
  void onHubLogonQueryForwardID(const muduo::net::TcpConnectionPtr& conn,
               const HubLogonQueryForwardIDPtr& message,
               muduo::Timestamp);
  void onHubConnectionData(const muduo::net::TcpConnectionPtr& conn,
                           const HubConnectionDataPtr& message,
                           muduo::Timestamp);
  void onHubKeeperLogonSessionRet(const muduo::net::TcpConnectionPtr& conn,
                                  const HubKeeperLogonSessionRetPtr& message,
                                  muduo::Timestamp);


  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;

  struct ForwardServerData
  {
    uint16_t id;
    std::string ip;
    uint16_t port;
    uint16_t numConnections;
    ForwardServerData(uint16_t i, const std::string& p, uint16_t pt, uint16_t num):id(i),ip(p),port(pt),numConnections(num)
    {
    }
    bool operator<(const ForwardServerData& rhs) const
    {
      return numConnections < rhs.numConnections;
    }
  };
  typedef uint16_t KeeperServerID;
  typedef std::map<KeeperServerID, std::list<ForwardServerData> > ForwardConnsMap;
  typedef ForwardConnsMap::iterator ForwardConnsMapIter;
  ForwardConnsMap forwardConns_;
  muduo::MutexLock forwardMutex_;

  typedef std::map<KeeperServerID, muduo::net::TcpConnectionPtr> KeeperConnsMap;
  KeeperConnsMap keeperConns_;
  muduo::net::TcpConnectionPtr logonConn_;
  muduo::MutexLock mutex_;

  template<typename MSG>
void sendToKeeper(KeeperServerID id, const MSG& msg)
{
  muduo::net::TcpConnectionPtr conn;
  {
    muduo::MutexLockGuard lock(mutex_);
    KeeperConnsMap::iterator it = keeperConns_.find(id);
    if (it != keeperConns_.end())
    {
      conn = it->second;
    }
  }
  if (conn)
  {
    codec_.send(conn, msg);
  }
}

  template<typename MSG>
void sendToLogon(const MSG& msg)
{
  muduo::net::TcpConnectionPtr conn;
  {
    muduo::MutexLockGuard lock(mutex_);
    conn = logonConn_;
  }
  if (conn)
  {
    codec_.send(conn, msg);
  }
}
};

#endif
