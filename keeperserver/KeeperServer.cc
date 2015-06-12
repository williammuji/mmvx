#include "KeeperServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include <sstream>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/ServerType.h>
#include <muduo/net/Sigaction.h>
#include <proto/hubKeeper.pb.h>
#include <proto/hub.pb.h>
using namespace muduo;
using namespace muduo::net;
using namespace keeperForward;

KeeperServer::KeeperServer(EventLoop* loop,
                           const InetAddress& listenAddr,
                           const InetAddress& hubAddr,
                           Lua* l,
                           uint16_t threadCount,
                           uint16_t serverID)
  : server_(loop, listenAddr, "KeeperServer"),
    dispatcher_(boost::bind(&KeeperServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l),
    serverID_(serverID),
    hubClient_(loop, hubAddr, "HubClient"),
    hubDispatcher_(boost::bind(&KeeperServer::onUnknownHubMessage, this, _1, _2, _3)),
    hubCodec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &hubDispatcher_, _1, _2, _3))
{
  assert(lua_);

  dispatcher_.registerMessageCallback<KeeperQuery>(
      boost::bind(&KeeperServer::onQuery, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<KeeperForwardLogonSessionRet>(
      boost::bind(&KeeperServer::onKeeperForwardLogonSessionRet, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<KeeperForwardUpdateForwardConns>(
      boost::bind(&KeeperServer::onKeeperForwardUpdateForwardConns, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&KeeperServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);

  hubDispatcher_.registerMessageCallback<HubKeeperLogonSession>(
      boost::bind(&KeeperServer::onHubKeeperLogonSession, this, _1, _2, _3));
  hubClient_.setConnectionCallback(
      boost::bind(&KeeperServer::onHubConnection, this, _1));
  hubClient_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &hubCodec_, _1, _2, _3));
  hubClient_.enableRetry();

  loadServerConfig();
}

void KeeperServer::loadServerConfig()
{
  serverConfig_.clear();
  uint16_t configCount = lua_->call<uint16_t>("getServerConfigCount");
  lua_tinker::table serverConfig = lua_->get<lua_tinker::table>("serverConfig");
  LOG_INFO << "KeeperServer::loadServerConfig configCount:" << configCount;

  for (uint16_t i=1; i<=configCount; ++i)
  {
    std::stringstream ss;
    ss << i;
    lua_tinker::table config = serverConfig.get<lua_tinker::table>(ss.str().c_str());

    ServerConfig sc;
    sc.id = config.get<uint16_t>("id"); 
    sc.ttype = config.get<uint16_t>("ttype"); 
    sc.ip = config.get<const char*>("ip"); 
    sc.port = config.get<uint16_t>("port"); 
    sc.threads = config.get<uint16_t>("threads");
    serverConfig_.push_back(sc);
  }
  LOG_INFO << "KeeperServer::loadServerConfig size:" << serverConfig_.size();
}

bool KeeperServer::isStarted(uint16_t serverID)
{
  for (size_t i=0; i<serverStart_.size(); ++i)
  {
    if (serverStart_[i].id == serverID && serverStart_[i].start) 
    {
      return true;
    }
  }
  return false;
}

void KeeperServer::setStarted(uint16_t serverID, bool start, const TcpConnectionPtr& conn)
{
  for (size_t i=0; i<serverStart_.size(); ++i)
  {
    if (serverStart_[i].id == serverID) 
    {
      serverStart_[i].start = start;
      return;
    }
  }
  serverStart_.push_back(ServerStart(serverID, start, conn->name()));
}

void KeeperServer::start()
{
  hubClient_.connect();
  server_.start();
}

void KeeperServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  }
  else
  {
    for (size_t i=0; i<serverStart_.size(); ++i)
    {
      if (serverStart_[i].connName == conn->name()
          && serverStart_[i].start == true)
      {
        HubKeeperUpdateForwardConns send;
        send.set_keeperserverid(serverID_);
        send.set_forwardserverid(serverStart_[i].id);
        for (size_t j=0; j<serverConfig_.size(); ++j)
        {
          if (serverConfig_[j].id == serverStart_[i].id)
          {
            send.set_forwardip(serverConfig_[j].ip);
            send.set_forwardport(serverConfig_[j].port);
            break;
          }
        }
        send.set_oper(HubKeeperUpdateForwardConns::REMOVE);
        sendToHub(send);

        setStarted(serverConfig_[i].id, false, conn);
        break;
      }
    }

    MutexLockGuard lock(mutex_);
    ConnectionMap::iterator it = conns_.begin();
    for ( ; it != conns_.end(); )
    {
      if (conn == it->second)
      {
        conns_.erase(it);
        break;
      }
      else
        ++it;
    }
  }
}

void KeeperServer::onHubConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    {
      MutexLockGuard lock(hubMutex_);
      hubConn_ = conn;
    }

    HubConnectionData data;
    data.set_servertype(SERVER_TYPE_KEEPER);
    data.set_serverid(serverID_);
    hubCodec_.send(conn, data);
  }
  else
  {
    MutexLockGuard lock(hubMutex_);
    hubConn_.reset();
  }
}

void KeeperServer::onUnknownHubMessage(const TcpConnectionPtr&,
                                       const MessagePtr& message,
                                       Timestamp)
{
  LOG_ERROR << "onUnknownHubMessage: " << message->GetTypeName();
}

void KeeperServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void KeeperServer::onQuery(const muduo::net::TcpConnectionPtr& conn,
                           const KeeperQueryPtr& message,
                           muduo::Timestamp)
{
  LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  for (size_t i=0; i<serverConfig_.size(); ++i)
  {
    if (serverConfig_[i].ttype == message->servertype()
        && muduo::string(serverConfig_[i].ip.c_str(), serverConfig_[i].ip.size()) == conn->peerAddress().toIp()
        && !isStarted(serverConfig_[i].id))
    {
      KeeperAnswer answer;
      answer.set_keeperserverid(serverID_);
      answer.set_serverid(serverConfig_[i].id);
      answer.set_ip(serverConfig_[i].ip);
      answer.set_port(serverConfig_[i].port);
      answer.set_threads(serverConfig_[i].threads);
      codec_.send(conn, answer);

      HubKeeperUpdateForwardConns send;
      send.set_keeperserverid(serverID_);
      send.set_forwardserverid(serverConfig_[i].id);
      send.set_forwardip(serverConfig_[i].ip);
      send.set_forwardport(serverConfig_[i].port);
      send.set_oper(HubKeeperUpdateForwardConns::ADD);
      sendToHub(send);

      setStarted(serverConfig_[i].id, true, conn);

      {
        MutexLockGuard lock(mutex_);
        conns_[serverConfig_[i].id] = conn;
      }
      return;
    }
  }

  LOG_ERROR << "Server Start Duplicate";
  conn->shutdown();
}

void KeeperServer::onKeeperForwardLogonSessionRet(const muduo::net::TcpConnectionPtr& conn,
                                                  const KeeperForwardLogonSessionRetPtr& message,
                                                  muduo::Timestamp)
{
  LOG_INFO << "onKeeperForwardLogonSessionRet:\n" << message->GetTypeName() << message->DebugString();

  HubKeeperLogonSessionRet send;
  send.set_uid(message->uid());
  send.set_connname(message->connname());
  send.set_forwardip(message->forwardip());
  send.set_forwardport(message->forwardport());
  send.set_session(message->session());
  sendToHub(send);
}

void KeeperServer::onHubKeeperLogonSession(const muduo::net::TcpConnectionPtr& conn,
                                           const HubKeeperLogonSessionPtr& message,
                                           muduo::Timestamp)
{
  LOG_INFO << "onHubKeeperLogonSession:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  KeeperForwardLogonSession send;
  send.set_uid(message->uid());
  send.set_connname(message->connname());
  sendToForward(message->forwardserverid(), send);
}

void KeeperServer::onKeeperForwardUpdateForwardConns(const muduo::net::TcpConnectionPtr& conn,
                                                     const KeeperForwardUpdateForwardConnsPtr& message,
                                                     muduo::Timestamp)
{
  LOG_INFO << "onKeeperForwardUpdateForwardConns:" << message->GetTypeName() << "  "  << message->DebugString();;

  HubKeeperUpdateForwardConns send;
  send.set_keeperserverid(message->keeperserverid());
  send.set_forwardserverid(message->forwardserverid());
  send.set_forwardip(message->forwardip());
  send.set_forwardport(message->forwardport());
  HubKeeperUpdateForwardConns::Operation oper = 
      static_cast<HubKeeperUpdateForwardConns::Operation>(message->oper());
  send.set_oper(oper);
  sendToHub(send);
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("keeperserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  lua_tinker::table keeperConfig = luaMgr.call<lua_tinker::table>("getKeeperServerConfig");
  const char* ip = keeperConfig.get<const char*>("ip");
  uint16_t port = keeperConfig.get<uint16_t>("port");
  uint16_t id = keeperConfig.get<uint16_t>("id");
  uint16_t threadCount = keeperConfig.get<uint16_t>("threads");

  lua_tinker::table hubConfig = luaMgr.call<lua_tinker::table>("getHubServerConfig");
  const char* hubIP = hubConfig.get<const char*>("ip");
  uint16_t hubPort = hubConfig.get<uint16_t>("port");

  InetAddress listenAddr(ip, port);
  InetAddress hubAddr(hubIP, hubPort);
  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  KeeperServer server(&loop, listenAddr, hubAddr, &luaMgr, threadCount, id);

  server.start();
  loop.loop();
}

