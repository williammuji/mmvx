#ifndef LOGON_SERVER_H
#define LOGON_SERVER_H

#include <proto/logon.pb.h>
#include <proto/hubLogon.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>
#include <muduo/net/TcpClient.h>
namespace muduo
{
class Lua;
}

typedef boost::shared_ptr<muduo::Logon> LogonPtr;
typedef boost::shared_ptr<muduo::LogonRet> LogonRetPtr;
typedef boost::shared_ptr<muduo::HubLogonQueryForwardID> HubLogonQueryForwardIDPtr;
typedef boost::shared_ptr<muduo::HubLogonAnswerForwardID> HubLogonAnswerForwardIDPtr;

class LogonServer : boost::noncopyable
{
 public:
  LogonServer(muduo::net::EventLoop* loop,
              const muduo::net::InetAddress& listenAddr,
              const muduo::net::InetAddress& hubAddr,
              muduo::Lua* l,
              uint16_t threadCount);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);
  void onHubClientConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);
  void onHubClientUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                                 const muduo::net::MessagePtr& message,
                                 muduo::Timestamp);

  void onLogon(const muduo::net::TcpConnectionPtr& conn,
               const LogonPtr& message,
               muduo::Timestamp);

  void onHubClientLogonAnswerForwardID(const muduo::net::TcpConnectionPtr& conn,
                                       const HubLogonAnswerForwardIDPtr& message,
                                       muduo::Timestamp);

  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::Lua* lua_;
  std::map<muduo::string, muduo::net::TcpConnectionPtr> clientConns_;
  muduo::MutexLock mutex_;

  muduo::net::TcpClient hubClient_;
  muduo::net::ProtobufDispatcher hubDispatcher_;
  muduo::net::ProtobufCodec hubCodec_;
  muduo::net::TcpConnectionPtr hubConn_;
  muduo::MutexLock hubMutex_;

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
void sendToClient(const muduo::string& connName, const MSG& msg)
{
  muduo::net::TcpConnectionPtr clientConn;
  {
    muduo::MutexLockGuard lock(mutex_);
    std::map<muduo::string, muduo::net::TcpConnectionPtr>::iterator it = clientConns_.find(connName);
    if (it != clientConns_.end())
    {
      clientConn = it->second;
    }
  }
  if (clientConn)
  {
    codec_.send(clientConn, msg);
  }
}
};

#endif
