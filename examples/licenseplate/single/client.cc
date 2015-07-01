#include <examples/licenseplate/single/licenseplate.pb.h>
#include <muduo/net/TcpClient.h>

#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <functional>
#include <set>
#include <map>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/base/Lua.h>

using namespace muduo;
using namespace muduo::net;
using namespace licenseplate;
typedef boost::shared_ptr<licenseplate::Logon> LogonPtr;
typedef boost::shared_ptr<licenseplate::LogonRet> LogonRetPtr;
typedef boost::shared_ptr<licenseplate::Bid> BidPtr;
typedef boost::shared_ptr<licenseplate::BidStatus> BidStatusPtr;

class Client;

class Session : boost::noncopyable
{
 public:
  Session(EventLoop* loop,
          const InetAddress& serverAddr,
          const string& name,
          Client* owner,
          int id)
    : loop_(loop),
      client_(loop, serverAddr, name),
      owner_(owner),
      id_(id),
      dispatcher_(boost::bind(&Session::onUnknownMessage, this, _1, _2, _3)),
      codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
      alertPrice_(0),
      minSuccessPrice_(0)
  {
    dispatcher_.registerMessageCallback<licenseplate::LogonRet>(
        boost::bind(&Session::onLogonRet, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<licenseplate::BidStatus>(
        boost::bind(&Session::onBidStatus, this, _1, _2, _3));
    client_.setConnectionCallback(
        boost::bind(&Session::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    client_.enableRetry();
  }

  void start()
  {
    client_.connect();
  }

  void stop()
  {
    client_.disconnect();
  }

 private:

  void onUnknownMessage(const TcpConnectionPtr& conn,
                        const MessagePtr& message,
                        Timestamp)
  {
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
    conn->shutdown();
  }


  void onLogonRet(const muduo::net::TcpConnectionPtr& conn,
                  const LogonRetPtr& message,
               muduo::Timestamp)
  {
    LOG_INFO << "onLogonRet:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();

    if (message->status() == licenseplate::LogonRet::FAILED)
      conn->shutdown();
    else if (message->status() == licenseplate::LogonRet::SUCCESS)
    {
      if (message->starttime() + message->lasttime()/2 > message->curtime())
      {
        int64_t firstBidTime = rand()%(message->starttime() + message->lasttime()/2 - message->curtime());
        if (firstBidTime == 0) firstBidTime = 1;
        loop_->runAfter(firstBidTime, boost::bind(&Session::firstHalfBid, this, conn));

        int64_t secondBidTime = rand()%(message->lasttime()/2) + message->starttime() + message->lasttime()/2 - message->curtime();
        loop_->runAfter(secondBidTime, boost::bind(&Session::secondHalfBid, this, conn));
        //secondBidTime = rand()%(message->lasttime()/2) + 1 + message->starttime() + message->lasttime()/2 - message->curtime();
        secondBidTime = message->starttime() + message->lasttime() - message->curtime() - rand()%30;
        loop_->runAfter(secondBidTime, boost::bind(&Session::secondHalfBid, this, conn));
      }
      else
        conn->shutdown();
    }
  }

  void onBidStatus(const muduo::net::TcpConnectionPtr& conn,
               const BidStatusPtr& message,
               muduo::Timestamp)
  {
    if (id_ <= 1)
    {
      LOG_INFO << "onBidStatus:\n" << message->GetTypeName() << message->DebugString();
      LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();
    }

    alertPrice_ = message->alertprice();
    minSuccessPrice_ = message->minsuccessprice();
  }

  void firstHalfBid(const TcpConnectionPtr& conn)
  {
    assert(alertPrice_ != 0);
    //uint32_t price = rand()%(alertPrice_/100)*100;
    //if (price < 100) price = 100;
    uint32_t price = alertPrice_;

    Bid bid;
    bid.set_id(id_);
    bid.set_code(id_);
    bid.set_price(price);
    codec_.send(conn, bid);
    LOG_INFO << "firstHalfBid:" << conn->name() << " id:" << id_ << " price:" << price;
  }

  void secondHalfBid(const TcpConnectionPtr& conn)
  {
    assert(minSuccessPrice_ != 0);
    uint32_t price = minSuccessPrice_ + 300;

    Bid bid;
    bid.set_id(id_);
    bid.set_code(id_);
    bid.set_price(price);
    codec_.send(conn, bid);
    LOG_INFO << "secondHalfBid:" << conn->name() << " id:" << id_ << " price:" << price;
  }

  void onConnection(const TcpConnectionPtr& conn);

  EventLoop* loop_;
  TcpClient client_;
  Client* owner_;
  int id_;
  ProtobufDispatcher dispatcher_;
  ProtobufCodec codec_;

  uint32_t alertPrice_;
  uint32_t minSuccessPrice_;
};

class Client : boost::noncopyable
{
 public:
  Client(EventLoop* loop,
         const InetAddress& serverAddr,
         int sessionCount,
         int threadCount)
    : loop_(loop),
      threadPool_(loop, "licensestate-client"),
      sessionCount_(sessionCount)
  {
    if (threadCount > 1)
    {
      threadPool_.setThreadNum(threadCount);
    }
    threadPool_.start();

    for (int i = 0; i < sessionCount; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "C%05d", i);
      Session* session = new Session(threadPool_.getNextLoop(), serverAddr, buf, this, i);
      session->start();
      sessions_.push_back(session);
    }

    srand(time(0));
  }

  void onConnect()
  {
    if (numConnected_.incrementAndGet() == sessionCount_)
    {
      LOG_WARN << "all connected";
    }
  }

  void onDisconnect(const TcpConnectionPtr& conn)
  {
    if (numConnected_.decrementAndGet() == 0)
    {
      LOG_WARN << "all disconnected";

      conn->getLoop()->queueInLoop(boost::bind(&Client::quit, this));
    }
  }

 private:

  void quit()
  {
    loop_->queueInLoop(boost::bind(&EventLoop::quit, loop_));
  }

  EventLoop* loop_;
  EventLoopThreadPool threadPool_;
  int sessionCount_;
  boost::ptr_vector<Session> sessions_;
  AtomicInt32 numConnected_;
};

void Session::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << conn->localAddress().toIpPort() << " -> "
      << conn->peerAddress().toIpPort() << " is "
      << (conn->connected() ? "UP" : "DOWN")
      << " id:" << id_
      << " name:" << conn->name();

  if (conn->connected())
  {
    conn->setTcpNoDelay(true);

    licenseplate::Logon logon;
    logon.set_id(id_);
    logon.set_passwd(id_);
    logon.set_code(id_);
    codec_.send(conn, logon);

    owner_->onConnect();
  }
  else
  {
    owner_->onDisconnect(conn);
  }
}

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("licenseplateclient", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::INFO);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  const char* ip = luaMgr.get<const char*>("LICENSE_PLATE_SERVER_IP");
  uint32_t port = luaMgr.get<uint32_t>("LICENSE_PLATE_SERVER_PORT");
  uint32_t sessionCount = luaMgr.get<uint32_t>("LICENSE_PLATE_CLIENTS");
  uint32_t threadCount = luaMgr.get<uint32_t>("LICENSE_PLATE_CLIENT_THREADS");
  InetAddress serverAddr(ip, port);

  EventLoop loop;
  Client client(&loop, serverAddr, sessionCount, threadCount);
  loop.loop();
}

