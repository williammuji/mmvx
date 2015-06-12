#include "ForwardClient.h"
#include <muduo/base/Logging.h>
#include <muduo/base/Lua.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/net/Sigaction.h>
using namespace muduo;
using namespace muduo::net;

ForwardClient::ForwardClient(muduo::net::EventLoop* loop,
                         const muduo::net::InetAddress& serverAddr,
                         const string& name,
                         int64_t uid,
                         uint32_t session)
: client_(loop, serverAddr, name),
    dispatcher_(boost::bind(&ForwardClient::onUnknownMessage, this, _1, _2, _3)),
    codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    uid_(uid),
    session_(session)
{
  client_.setConnectionCallback(
      boost::bind(&ForwardClient::onConnection, this, _1));
  client_.setMessageCallback(
      boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

  //enable retry
}

void ForwardClient::start()
{
  client_.connect();
}

void ForwardClient::stop()
{
  client_.disconnect();
}

void ForwardClient::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    conn->setTcpNoDelay(true);

    muduo::LogonForward query;
    query.set_uid(uid_);
    query.set_session(session_);
    codec_.send(conn, query);
  }
  else
  {
  }
}

void ForwardClient::onUnknownMessage(const TcpConnectionPtr& conn,
                                   const MessagePtr& message,
                                   Timestamp)
{
  LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();

  conn->shutdown();
}
