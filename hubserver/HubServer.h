#ifndef HUB_SERVER_H
#define HUB_SERVER_H

#include <proto/hubKeeper.pb.h>
#include <proto/hubLogon.pb.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::HubKeeperUpdateForwardConns> HubKeeperUpdateForwardConnsPtr;
typedef boost::shared_ptr<muduo::HubLogonQueryForwardID> HubLogonQueryForwardIDPtr;

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

  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;

  struct ForwardServerData
  {
    uint16_t id;
    std::string ip;
    uint16_t port;
    uint16_t conns;
    ForwardServerData(uint16_t i, const std::string& p, uint16_t pt, uint16_t cn):id(i),ip(p),port(pt),conns(cn)
    {
    }
    bool operator<(const ForwardServerData& rhs) const
    {
      return conns < rhs.conns;
    }
  };
  typedef uint16_t KeeperServerID;
  typedef std::map<KeeperServerID, std::vector<ForwardServerData> > ForwardConnsMap;
  typedef ForwardConnsMap::iterator ForwardConnsMapIter;
  ForwardConnsMap forwardConns_;
};

#endif
