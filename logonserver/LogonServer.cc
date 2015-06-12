#include "LogonServer.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/LogFile.h>
#include <muduo/net/ServerType.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
#include <proto/hub.pb.h>
using namespace muduo;
using namespace muduo::net;

LogonServer::LogonServer(EventLoop* loop,
                         const InetAddress& listenAddr,
                         const InetAddress& hubAddr,
                         Lua* l,
                         uint16_t threadCount,
                         uint16_t serverID)
  : server_(loop, listenAddr, "LogonServer"),
    dispatcher_(boost::bind(&LogonServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    lua_(l),
    serverID_(serverID),
    hubClient_(loop, hubAddr, "HubClient"),
    hubDispatcher_(boost::bind(&LogonServer::onHubClientUnknownMessage, this, _1, _2, _3)),
    hubCodec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &hubDispatcher_, _1, _2, _3)),
    mysqlConnectionPool_("test", "127.0.0.1", "noahsark", "noahsark", 4)
{
  assert(lua_);

  dispatcher_.registerMessageCallback<muduo::Logon>(
      boost::bind(&LogonServer::onLogon, this, _1, _2, _3));
  server_.setConnectionCallback(
      boost::bind(&LogonServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  server_.setThreadNum(threadCount);

  hubDispatcher_.registerMessageCallback<muduo::HubLogonAnswerForwardID>(
      boost::bind(&LogonServer::onHubLogonAnswerForwardID, this, _1, _2, _3));
  hubClient_.setConnectionCallback(
      boost::bind(&LogonServer::onHubClientConnection, this, _1));
  hubClient_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &hubCodec_, _1, _2, _3));
  hubClient_.enableRetry();
}

void LogonServer::start()
{
  hubClient_.connect();
  server_.start();
}

void LogonServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  MutexLockGuard lock(mutex_);
  if (conn->connected())
  {
    clientConns_[conn->name()] = conn;
  }
  else
  {
    clientConns_.erase(conn->name());
  }
}

void LogonServer::onHubClientConnection(const TcpConnectionPtr& conn)
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
    data.set_servertype(SERVER_TYPE_LOGON);
    data.set_serverid(serverID_);
    hubCodec_.send(conn, data);
  }
  else
  {
    MutexLockGuard lock(hubMutex_);
    hubConn_.reset();
  }
}

void LogonServer::onUnknownMessage(const TcpConnectionPtr& conn,
                                   const MessagePtr& message,
                                   Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void LogonServer::onHubClientUnknownMessage(const TcpConnectionPtr& conn,
                                            const MessagePtr& message,
                                            Timestamp)
{
  LOG_INFO << "onHubClientUnknownMessage: " << message->GetTypeName();
  conn->shutdown();
}

void LogonServer::queryLogonAccount(uint32_t uid, const StringPiece& passwd, uint32_t keeperServerID, const StringPiece& connName)
{
  mysqlpp::ScopedConnection cp(mysqlConnectionPool_, true);
  if (!cp)
  {
    LOG_ERROR << "Failed to get a connection from the pool! uid:" << uid << " connName:" << connName.data();
    return;
  }

  std::stringstream queryStr;
  queryStr << "select * from ACCOUNT where UID=" << uid;
  mysqlpp::Query query(cp->query(queryStr.str()));
  mysqlpp::StoreQueryResult res = query.store();
  if (res.num_rows() == 1 && res[0]["PASSWD"] == passwd.data())
  {
    LOG_INFO << "PASSWD:" << res[0]["PASSWD"].data();
    HubLogonQueryForwardID msg;
    msg.set_uid(uid);
    msg.set_connname(connName.data(), connName.size());
    msg.set_keeperserverid(keeperServerID);
    sendToHub(msg);
    LOG_INFO << "LogonServer::queryLogonAccount uid:" << uid << " passwd:" << passwd << " keeperServerID:" << keeperServerID << " connName:" << connName;
  }
  else
  {
    LogonRet ret;
    ret.set_uid(uid);
    ret.set_status("FAILED");
    sendToClient(connName.data(), ret);
    shutdownConn(connName.data());
    LOG_INFO << "LogonServer::queryLogonAccount FAILED uid:" << uid << " passwd:" << passwd << " keeperServerID:" << keeperServerID << " connName:" << connName;
  }
}

void LogonServer::onLogon(const muduo::net::TcpConnectionPtr& conn,
                          const LogonPtr& message,
                          muduo::Timestamp)
{
  LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
  LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

  mysqlConnectionPool_.put(
      boost::bind(&LogonServer::queryLogonAccount, this, message->uid(), message->passwd(), message->keeperserverid(), conn->name())); 
}

void LogonServer::onHubLogonAnswerForwardID(const muduo::net::TcpConnectionPtr& conn,
                                                  const HubLogonAnswerForwardIDPtr& message,
                                                  muduo::Timestamp)
{
  LOG_INFO << "onHubClientLogonAnswerForwardID:\n" << message->GetTypeName() << message->DebugString();

  //TODO send to client forward IP&port
  LogonRet ret;
  ret.set_uid(message->uid());
  ret.set_status("SUCCESS");
  ret.set_forwardip(message->forwardip());
  ret.set_forwardport(message->forwardport());
  ret.set_session(message->session());

  sendToClient(message->connname().c_str(), ret);
  LOG_INFO << "onHubClientLogonAnswerForwardID uid:" << message->uid() << " forwardip:" << message->forwardip() << " forwardport:" << message->forwardport();
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("logonserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::TRACE);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");

  lua_tinker::table logonConfig = luaMgr.call<lua_tinker::table>("getLogonServerConfig");
  const char* ip = logonConfig.get<const char*>("ip");
  uint16_t port = logonConfig.get<uint16_t>("port");
  uint16_t threadCount = logonConfig.get<uint16_t>("threads");
  uint16_t id = logonConfig.get<uint16_t>("id");
  InetAddress listenAddr(ip, port);

  lua_tinker::table hubConfig = luaMgr.call<lua_tinker::table>("getHubServerIpPort");
  const char* hubIP = hubConfig.get<const char*>("ip");
  uint16_t hubPort = hubConfig.get<uint16_t>("port");
  InetAddress hubAddr(hubIP, hubPort);

  EventLoop loop;
  muduo::net::setMainEventLoop(&loop);
  LogonServer server(&loop, listenAddr, hubAddr, &luaMgr, threadCount, id);

  server.start();
  loop.loop();
}

