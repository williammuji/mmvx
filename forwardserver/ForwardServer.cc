#include "ForwardServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include "KeeperClient.h"
#include <sstream>
#include <proto/keeperForward.pb.h>
#include <stdlib.h>
using namespace muduo;
using namespace muduo::net;
using namespace keeperForward;

ForwardServer::ForwardServer(EventLoop* loop,
                           const InetAddress& listenAddr,
                           Lua* l,
                           uint16_t threadCount,
                           uint16_t keeperServerID,
                           uint16_t serverID,
                           KeeperClient* client)

  : loop_(loop),
    server_(loop, listenAddr, "ForwardServer"),
    listenAddr_(listenAddr),
    dispatcher_(boost::bind(&ForwardServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l),
    keeperServerID_(keeperServerID),
    serverID_(serverID),
    client_(CHECK_NOTNULL(client))
{
<<<<<<< HEAD
  codec_.setAes();

=======
>>>>>>> b318cba5298df589a32ab01efdb3f54d2d4e86fc
  dispatcher_.registerMessageCallback<muduo::LogonForward>(
      boost::bind(&ForwardServer::onLogonForward, this, _1, _2, _3));
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

  KeeperForwardUpdateForwardConns send;
  send.set_keeperserverid(keeperServerID_);
  send.set_forwardserverid(serverID_);
  send.set_forwardip(listenAddr_.toIp().c_str());
  send.set_forwardport(listenAddr_.toPort());
  if (conn->connected())
  {
    send.set_oper(KeeperForwardUpdateForwardConns::INC);
  }
  else
  {
    send.set_oper(KeeperForwardUpdateForwardConns::DEC);
  }
  client_->sendToKeeper(send);
}

void ForwardServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                     const MessagePtr& message,
                                    Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void ForwardServer::onLogonForward(const muduo::net::TcpConnectionPtr& conn,
                                   const LogonForwardPtr& message,
                                   muduo::Timestamp ts)
{
  LOG_INFO << "onLogonForward:\n" << message->GetTypeName() << message->DebugString();

  loop_->queueInLoop(boost::bind(&ForwardServer::checkLogonForwardSession, this, conn, message, ts));
}

void ForwardServer::checkLogonForwardSession(const muduo::net::TcpConnectionPtr& conn,
                                             const LogonForwardPtr& message,
                                             muduo::Timestamp)
{
  LOG_INFO << "checkLogonForwardSession:\n" << message->GetTypeName() << message->DebugString();

  loop_->assertInLoopThread();
 
  LogonSessionsMap::iterator it = logonSessions_.find(message->uid());
  if (it != logonSessions_.end() && it->second == message->session())
  {
    LOG_INFO << "LogonForward SUCCESS uid:" << message->uid() << " session:" << message->session() << " " << conn->name();
  }
  else
  {
    LOG_INFO << "LogonForward FAILED uid:" << message->uid() << " session:" << message->session() << " " << conn->name();
    conn->shutdown();
  }

  if (it != logonSessions_.end())
    logonSessions_.erase(it);
}

void ForwardServer::addLogonSession(int64_t uid, int32_t session)
{
  loop_->assertInLoopThread();

  logonSessions_[uid] = session;
}
