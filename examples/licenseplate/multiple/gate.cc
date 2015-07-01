#include <examples/licenseplate/multiple/licenseplate.pb.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>
#include <muduo/base/StringPiece.h>
#include <functional> 
#include <set>
#include <map>
#include <boost/bind.hpp>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/base/LoggerOutput.h>
#include <muduo/base/Lua.h>
#include <muduo/net/TcpClient.h>
#include <boost/scoped_ptr.hpp>
#include <muduo/base/ThreadLocal.h>
using namespace muduo;
using namespace muduo::net;
using namespace licenseplate;

typedef boost::shared_ptr<licenseplate::Logon> LogonPtr;
typedef boost::shared_ptr<licenseplate::LogonRet> LogonRetPtr;
typedef boost::shared_ptr<licenseplate::Bid> BidPtr;
typedef boost::shared_ptr<licenseplate::BidStatus> BidStatusPtr;

class Backend : boost::noncopyable
{
 public:
  Backend(EventLoop* loop, const InetAddress& backendAddr, const string& name)
      : loop_(loop),
      client_(loop, backendAddr, name),
      dispatcher_(boost::bind(&Backend::onUnknownMessage, this, _1, _2, _3)),
      codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
  {
    dispatcher_.registerMessageCallback<licenseplate::LogonRet>(
        boost::bind(&Backend::onLogonRet, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<licenseplate::BidStatus>(
        boost::bind(&Backend::onBidStatus, this, _1, _2, _3));
    client_.setConnectionCallback(
        boost::bind(&Backend::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    //client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

  void addClientConn(uint32_t id, const TcpConnectionPtr& conn)
  {
    loop_->assertInLoopThread();
    conn->setContext(id);
    clientConns_[id] = conn;
    LOG_INFO << "addClientConn id:" << id << " conn:" << conn->name() << " clientConns_.size:" << clientConns_.size();
  }

  void removeClientConn(const TcpConnectionPtr& conn)
  {
    loop_->assertInLoopThread();
    if (!conn->getContext().empty())
    {
      uint32_t id = boost::any_cast<uint32_t>(conn->getContext());
      std::map<uint32_t, muduo::net::TcpConnectionPtr>::iterator it = 
          clientConns_.find(id);
      if (it != clientConns_.end())
      {
        clientConns_.erase(it);
      }

      Logout logout;
      logout.set_id(id);
      send(logout);
    }
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    loop_->assertInLoopThread();
    LOG_INFO << "Backend "
        << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
    if (conn->connected())
    {
      conn_ = conn;
    }
    else
    {
      conn_.reset();
    }
  }

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp)
  {
    conn->shutdown();
    LOG_ERROR << "Backend::onUnknownMessage:\n" << message->GetTypeName() << message->DebugString();
  }

  void onLogonRet(const muduo::net::TcpConnectionPtr& conn,
                  const LogonRetPtr& message,
                  muduo::Timestamp)
  {
    loop_->assertInLoopThread();

    sendToClient(*message);
  }

  void onBidStatus(const muduo::net::TcpConnectionPtr& conn,
                   const BidStatusPtr& message,
                   muduo::Timestamp)
  {
    loop_->assertInLoopThread();
    sendToClient(*message);
  }

  EventLoop* loop_;
  TcpClient client_;
  TcpConnectionPtr conn_;
  ProtobufDispatcher dispatcher_;
  ProtobufCodec codec_;
  std::map<uint32_t, muduo::net::TcpConnectionPtr> clientConns_;

 public:
  template<typename MSG>
void send(const MSG& msg)
{
  loop_->assertInLoopThread();
  if (conn_)
    codec_.send(conn_, msg);
}

  template<typename MSG>
void sendToClient(const MSG& msg)
{
  loop_->assertInLoopThread();
  std::map<uint32_t, muduo::net::TcpConnectionPtr>::iterator it =
      clientConns_.find(msg.id());
  if (it != clientConns_.end())
    codec_.send(it->second, msg);
}

};



class GateServer : boost::noncopyable
{
 public:
  GateServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr,
             const muduo::net::InetAddress& backendAddr,
             uint32_t threadCount)
      : loop_(loop), 
      server_(loop, listenAddr, "GateServer"),
      dispatcher_(boost::bind(&GateServer::onUnknownMessage, this, _1, _2, _3)),
      codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
      backendAddr_(backendAddr)
  {
    dispatcher_.registerMessageCallback<licenseplate::Logon>(
        boost::bind(&GateServer::onLogon, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<licenseplate::Bid>(
        boost::bind(&GateServer::onBid, this, _1, _2, _3));
    server_.setThreadInitCallback(
        boost::bind(&GateServer::initPerThread, this, _1));
    server_.setConnectionCallback(
        boost::bind(&GateServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_.setThreadNum(threadCount);
  }

  void start()
  {
    server_.start();
  }

 private:
  struct PerThread
  {
    boost::scoped_ptr<Backend> backend;
  };

  void initPerThread(EventLoop* ioLoop)
  {
    int count = threadCount_.getAndAdd(1);
    LOG_INFO << "IO thread " << count;
    PerThread& t = backend_.value();

    char buf[32];
    snprintf(buf, sizeof buf, "%s#%d", backendAddr_.toIpPort().c_str(), count);
    t.backend.reset(new Backend(ioLoop, backendAddr_, buf));
    t.backend->connect();
  }

  void onConnection(const muduo::net::TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN")
        << " name:" << conn->name();

    if (conn->connected())
    {
      conn->setTcpNoDelay(true);
    }
    else
    {
      PerThread& t = backend_.value();
      t.backend->removeClientConn(conn);
    }
  }

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp)
  {
    //conn->shutdown();
    LOG_ERROR << "GateServer::onUnknownMessage:\n" << message->GetTypeName() << message->DebugString();
  }

  void onLogon(const muduo::net::TcpConnectionPtr& conn,
               const LogonPtr& message,
               muduo::Timestamp cur)
  {
    LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort() << " name:" << conn->name();

    PerThread& t = backend_.value();
    t.backend->addClientConn(message->id(), conn);
    t.backend->send(*message);
  }

  void onBid(const muduo::net::TcpConnectionPtr& conn,
             const BidPtr& message,
             muduo::Timestamp cur)
  {
    LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort() << " name:" << conn->name();

    PerThread& t = backend_.value();
    t.backend->send(*message);
  }


  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  muduo::net::InetAddress backendAddr_;
  ThreadLocal<PerThread> backend_;
  AtomicInt32 threadCount_;
};

int main(int argc, char* argv[])
{
  if (argc != 6)
  {
    printf("Usage: %s ip port threads\n", argv[0]);
    return 0;
  }


  muduo::setLoggerOutputFile("licenseplategate", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::INFO);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  daemon(1, 1);

  InetAddress listenAddr(argv[1], atoi(argv[2]));
  InetAddress backendAddr(argv[3], atoi(argv[4]));

  EventLoop loop;
  GateServer server(&loop, listenAddr, backendAddr, atoi(argv[5]));

  server.start();
  loop.loop();
}
