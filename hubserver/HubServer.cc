#include "HubServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/LogFile.h>
#include <sstream>
#include <limits>
#include <muduo/net/ServerType.h>
#include <algorithm>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>

using namespace muduo;
using namespace muduo::net;


HubServer::HubServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     Lua* l,
                     uint16_t threadCount)
  : server_(loop, listenAddr, "HubServer"),
    dispatcher_(boost::bind(&HubServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l)
{
  assert(lua_);

  dispatcher_.registerMessageCallback<muduo::HubKeeperUpdateForwardConns>(
      boost::bind(&HubServer::onHubKeeperUpdateForwardConns, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::HubLogonQueryForwardID>(
      boost::bind(&HubServer::onHubLogonQueryForwardID, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::HubConnectionData>(
      boost::bind(&HubServer::onHubConnectionData, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::HubKeeperLogonSessionRet>(
      boost::bind(&HubServer::onHubKeeperLogonSessionRet, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&HubServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);
}

void HubServer::start()
{
  server_.start();
}

void HubServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  }
  else
  {
    MutexLockGuard lock(mutex_);
    if (logonConn_ == conn)
      logonConn_.reset();
    else
    {
      KeeperConnsMap::iterator it = keeperConns_.begin();
      for ( ; it != keeperConns_.end(); )
      {
        if (conn == it->second)
        {
          keeperConns_.erase(it);
          break;
        }
        else
          ++it;
      }
    }
  }
}

void HubServer::onHubConnectionData(const muduo::net::TcpConnectionPtr& conn,
                                    const HubConnectionDataPtr& message,
                                    muduo::Timestamp)
{
  LOG_INFO << "onHubConnectionData:\n" << message->GetTypeName() << message->DebugString();

  muduo::MutexLockGuard lock(mutex_);
  if (message->servertype() == SERVER_TYPE_LOGON)
    logonConn_ = conn;
  else if (message->servertype() == SERVER_TYPE_KEEPER)
  {
    keeperConns_[message->serverid()] = conn;
  }
}

void HubServer::onHubKeeperLogonSessionRet(const muduo::net::TcpConnectionPtr& conn,
                                           const HubKeeperLogonSessionRetPtr& message,
                                           muduo::Timestamp)
{
  LOG_INFO << "onHubKeeperLogonSessionRet:\n" << message->GetTypeName() << message->DebugString();

  HubLogonAnswerForwardID answer;
  answer.set_uid(message->uid());
  answer.set_connname(message->connname());
  answer.set_forwardip(message->forwardip());
  answer.set_forwardport(message->forwardport());
  answer.set_session(message->session());
  sendToLogon(answer);
}

void HubServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void HubServer::onHubKeeperUpdateForwardConns(const muduo::net::TcpConnectionPtr& conn,
                           const HubKeeperUpdateForwardConnsPtr& message,
                           muduo::Timestamp)
{
  LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  KeeperServerID keeperID = message->keeperserverid();
  uint16_t forwardID = message->forwardserverid();
  std::string forwardIP = message->forwardip();
  uint16_t forwardPort = message->forwardport();
  muduo::HubKeeperUpdateForwardConns::Operation oper = message->oper();

  MutexLockGuard lock(forwardMutex_);
  std::list<ForwardServerData>& forwardList = forwardConns_[keeperID];
  if (oper == muduo::HubKeeperUpdateForwardConns::ADD)
      forwardList.push_back(ForwardServerData(forwardID, forwardIP, forwardPort, 0));
  else
  {
    std::list<ForwardServerData>::iterator it = forwardList.begin();
    for ( ; it != forwardList.end(); ++it)
    {
      if ((*it).id == forwardID)
      {
        if (oper == muduo::HubKeeperUpdateForwardConns::REMOVE)
          forwardList.erase(it);
        else if (oper == muduo::HubKeeperUpdateForwardConns::INC)
         ++(*it).numConnections;
        else if (oper == muduo::HubKeeperUpdateForwardConns::DEC)
         --(*it).numConnections; 

        LOG_INFO << "KeeperID:" << keeperID << " ForwardID:" << forwardID << " numConnections:" << (*it).numConnections; 
        break;
      }
    }
  }
}

void HubServer::onHubLogonQueryForwardID(const muduo::net::TcpConnectionPtr& conn,
                           const HubLogonQueryForwardIDPtr& message,
                           muduo::Timestamp)
{
  LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  uint32_t keeperServerID = message->keeperserverid();

  MutexLockGuard lock(forwardMutex_);
  ForwardConnsMapIter iter = forwardConns_.find(keeperServerID);
  if (iter != forwardConns_.end() && !iter->second.empty())
  {
    std::list<ForwardServerData>::iterator it =
        std::min_element(iter->second.begin(), iter->second.end());

    HubKeeperLogonSession send;
    send.set_uid(message->uid());
    send.set_connname(message->connname());
    send.set_forwardserverid(it->id);
    sendToKeeper(keeperServerID, send);
  }
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("hubserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  lua_tinker::table hubConfig = luaMgr.call<lua_tinker::table>("getHubServerConfig");
  const char* ip = hubConfig.get<const char*>("ip");
  uint16_t port = hubConfig.get<uint16_t>("port");
  uint16_t threadCount = hubConfig.get<uint16_t>("threads");

  InetAddress listenAddr(ip, port);
  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  HubServer server(&loop, listenAddr, &luaMgr, threadCount);

  server.start();
  loop.loop();
}

