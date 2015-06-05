#include "KeeperServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include <sstream>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/ServerType.h>
#include <muduo/net/Sigaction.h>
using namespace muduo;
using namespace muduo::net;


KeeperServer::KeeperServer(EventLoop* loop,
                           const InetAddress& listenAddr,
                           Lua* l,
                           uint16_t threadCount)
  : server_(loop, listenAddr, "KeeperServer"),
    dispatcher_(boost::bind(&KeeperServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l)
{
  assert(lua_);

  dispatcher_.registerMessageCallback<muduo::KeeperQuery>(
      boost::bind(&KeeperServer::onQuery, this, _1, _2, _3));
  dispatcher_.registerMessageCallback<muduo::KeeperAnswer>(
      boost::bind(&KeeperServer::onAnswer, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&KeeperServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);

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

void KeeperServer::setStarted(uint16_t serverID)
{
  for (size_t i=0; i<serverStart_.size(); ++i)
  {
    if (serverStart_[i].id == serverID) 
    {
      serverStart_[i].start = true;
      return;
    }
  }
  serverStart_.push_back(ServerStart(serverID, true));
}

void KeeperServer::start()
{
  server_.start();
}

void KeeperServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");
}

void KeeperServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                    const MessagePtr& message,
                                    Timestamp)
{
  LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
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
      answer.set_serverid(serverConfig_[i].id);
      answer.set_ip(serverConfig_[i].ip);
      answer.set_port(serverConfig_[i].port);
      answer.set_threads(serverConfig_[i].threads);
      codec_.send(conn, answer);

      setStarted(serverConfig_[i].id);
      return;
    }
  }

  LOG_INFO << "Server Start Duplicate";
  conn->shutdown();
}

void KeeperServer::onAnswer(const muduo::net::TcpConnectionPtr& conn,
                            const KeeperAnswerPtr& message,
                            muduo::Timestamp)
{
  LOG_INFO << "onAnswer: " << message->GetTypeName();
  conn->shutdown();
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
  uint16_t threadCount = keeperConfig.get<uint16_t>("threads");

  InetAddress listenAddr(ip, port);
  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  KeeperServer server(&loop, listenAddr, &luaMgr, threadCount);

  server.start();
  loop.loop();
}

