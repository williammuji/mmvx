#include <examples/licenseplate/single/licenseplate.pb.h>

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
using namespace muduo;
using namespace muduo::net;
using namespace licenseplate;

typedef boost::shared_ptr<licenseplate::Logon> LogonPtr;
typedef boost::shared_ptr<licenseplate::Bid> BidPtr;

struct Session
{
  struct SessionData 
  {
    SessionData(Timestamp t, uint32_t p)
        : ts(t), price(p){}
    Timestamp ts;
    uint32_t price;
  };
  std::vector<SessionData> datas_; 
};


class LicensePlateServer : boost::noncopyable
{
 public:
  LicensePlateServer(muduo::net::EventLoop* loop,
                     const muduo::net::InetAddress& listenAddr,
                     uint32_t threadCount,
                     uint32_t halfSeconds,
                     uint32_t maxLicenses,
                     uint32_t alertPrice)
      : loop_(loop), 
      server_(loop, listenAddr, "LicensePlateServer"),
      dispatcher_(boost::bind(&LicensePlateServer::onUnknownMessage, this, _1, _2, _3)),
      codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
      halfSeconds_(halfSeconds),
      maxLicenses_(maxLicenses),
      alertPrice_(alertPrice),
      minPrice_(100),
      minSameTimeNum_(0)
  {
    dispatcher_.registerMessageCallback<licenseplate::Logon>(
        boost::bind(&LicensePlateServer::onLogon, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<licenseplate::Bid>(
        boost::bind(&LicensePlateServer::onBid, this, _1, _2, _3));
    server_.setConnectionCallback(
        boost::bind(&LicensePlateServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_.setThreadNum(threadCount);

    assert(alertPrice_%100 == 0);
    assert(maxLicenses_ >= 2);
  }


  void start()
  {
    server_.start();

    loop_->runAfter(halfSeconds_, boost::bind(&LicensePlateServer::halfSecondsOut, this));
    loop_->runAfter(halfSeconds_*2, boost::bind(&LicensePlateServer::timeout, this));
    loop_->runEvery(1.0, boost::bind(&LicensePlateServer::everySecond, this));

    start_ = Timestamp::now();
    half_ = addTime(start_, halfSeconds_);
    end_ = addTime(start_, 2*halfSeconds_);
  }

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN")
        << " name:" << conn->name();

    MutexLockGuard lock(mutex_);
    if (conn->connected())
    {
      conn->setTcpNoDelay(true);

      connections_[conn->name()] = conn;
      LOG_INFO << "onConnection size:" << connections_.size();
    }
    else
    {
      connections_.erase(conn->name());
      if (connections_.empty()) 
        loop_->runAfter(3, boost::bind(&EventLoop::quit, loop_));
    }
  }

  void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp)
  {
    conn->shutdown();
    LOG_INFO << "logonInLoop:\n" << message->GetTypeName() << message->DebugString();
  }

  void onLogon(const muduo::net::TcpConnectionPtr& conn,
               const LogonPtr& message,
               muduo::Timestamp cur)
  {
    LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();
    LOG_INFO << "name:" << conn->name();

    if (cur <= end_ && start_ < cur)
    {
      loop_->queueInLoop(boost::bind(&LicensePlateServer::logonInLoop, this, conn, message, cur));
    }
    else
      conn->shutdown();
  }

  void logonInLoop(const muduo::net::TcpConnectionPtr& conn,
                   const LogonPtr& message,
                   muduo::Timestamp cur)
  {
    LOG_INFO << "logonInLoop:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "name:" << conn->name();

    //verify id&passwd&code
    
    uint32_t id = message->id();
    conn->setContext(id);

    LogonRet ret;
    ret.set_id(id);
    if (secondHalf(cur))
    {
      if (sessions_.find(id) == sessions_.end())
      {
        LOG_ERROR << "onLong FAILED: user hadn't bid at the first half, id:" << id;

        ret.set_status(licenseplate::LogonRet::FAILED);
        ret.set_starttime(0);
        ret.set_lasttime(0);
        ret.set_curtime(0);
        codec_.send(conn, ret);
        conn->shutdown();
        return;
      }
    }

    ret.set_status(licenseplate::LogonRet::SUCCESS);
    ret.set_starttime(start_.secondsSinceEpoch());
    ret.set_lasttime(halfSeconds_*2);
    int64_t ts = Timestamp::now().secondsSinceEpoch();
    ret.set_curtime(ts);
    codec_.send(conn, ret);

    sendBidStatus(conn);
  }

  void onBid(const muduo::net::TcpConnectionPtr& conn,
                 const BidPtr& message,
                 muduo::Timestamp cur)
  {
    LOG_INFO << "onLogon:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "peerAddress" << conn->peerAddress().toIpPort();
    LOG_INFO << "name:" << conn->name();

    loop_->queueInLoop(boost::bind(&LicensePlateServer::bidInLoop, this, conn, message, cur));
  }

  bool firstHalf(Timestamp cur) const
  {
    return cur <= half_ && start_ < cur;
  }
  bool secondHalf(Timestamp cur) const
  {
    return cur <= end_ && half_ < cur;
  }
  bool checkFirstHalfPrice(uint32_t price)
  {
    if (price == 0
        || price%100 != 0
        || price > alertPrice_) return false;
    else
      return true;
  }
  bool checkSecondHalfPrice(uint32_t price)
  {
    if (price == 0
        || price%100 != 0) return false;

    if (minPrice_ >= 300)
    {
      if (price < minPrice_-300 || price > minPrice_+300)
      {
        return false;
      }
    }
    else
    {
      if (price > minPrice_+300)
      {
        return false;
      }
    }
    return true;
  }

  void setMinPrice()
  {
    if (bids_.size() < maxLicenses_)
    {
      minPrice_ = 100;
      minSameTimeNum_ = 0;
    }
    else
    {
      BidSet::iterator it = bids_.begin();
      std::advance(it, maxLicenses_-1);
      minPrice_ = it->price;
      minSameTimeNum_ = 1;
      minSameTime_ = it->ts;
      time_t seconds = minSameTime_.secondsSinceEpoch();
      BidSet::iterator forwardIt = --it;
      while (forwardIt->ts.secondsSinceEpoch() == seconds)
      {
        ++minSameTimeNum_;
        if (forwardIt == bids_.begin()) break;
        --forwardIt;
      }
    }
  }

  void bidInLoop(const muduo::net::TcpConnectionPtr& conn,
                 const BidPtr& message,
                 muduo::Timestamp cur)
  {
    LOG_INFO << "logonInLoop:\n" << message->GetTypeName() << message->DebugString();
    LOG_INFO << "name:" << conn->name();

    uint32_t id = message->id();
    if (firstHalf(cur))
    {
      SessionMap::iterator it = sessions_.find(id);
      if (it != sessions_.end())
      {
        LOG_ERROR << "bid FAILED: only once bid at the first half time";
        return;
      }
      else
      {
        uint32_t price = message->price();
        if (!checkFirstHalfPrice(price))
        {
          LOG_ERROR << "bid FAILED: wrong price at the FIRST half time, bidPrice:" << price << " minPrice:" << minPrice_ << " minSameTimeNum:" << minSameTimeNum_ << " minSameTime:" << minSameTime_.toFormattedString() << " bidTime:" << cur.toFormattedString();
          return;
        }

        Session session;
        session.datas_.push_back(Session::SessionData(cur, price));
        sessions_[id] = session;

        BidData bidData;
        bidData.id = id;
        bidData.price = price;
        bidData.ts = cur;
        bids_.insert(bidData);

        setMinPrice();
        LOG_INFO << "bid SUCCESS: at the FIRST half time, bidPrice:" << price << " minPrice:" << minPrice_ << " minSameTimeNum:" << minSameTimeNum_ << " minSameTime:" << minSameTime_.toFormattedString() << " bidTime:" << cur.toFormattedString();
      }     
    }
    else if (secondHalf(cur))
    {
      SessionMap::iterator it = sessions_.find(id);
      if (it == sessions_.end())
      {
        LOG_ERROR << "bid FAILED: must bid once at the first half time";
        return;
      }
      else
      {
        if (it->second.datas_.size() >= 3)
        {
          LOG_ERROR << "bid FAILED: only twice bid at the second half time";
          return;
        }
        else
        {
          uint32_t price = message->price();
          if (!checkSecondHalfPrice(price))
          {
            LOG_ERROR << "bid FAILED: wrong price at the SECOND half time, bidPrice:" << price << " minPrice:" << minPrice_ << " minSameTimeNum:" << minSameTimeNum_ << " minSameTime:" << minSameTime_.toFormattedString() << " bidTime:" << cur.toFormattedString();
            return;
          }

          it->second.datas_.push_back(Session::SessionData(cur, price));

          BidData bidData;
          bidData.id = id;
          bidData.price = price;
          bidData.ts = cur;
          BidSet::iterator bidIter = bids_.find(bidData);
          if (bidIter != bids_.end())
            bids_.erase(bidIter);
          bids_.insert(bidData);

          setMinPrice();
          LOG_INFO << "bid SUCCESS: at the SECOND half time, bidPrice:" << price << " minPrice:" << minPrice_ << " minSameTimeNum:" << minSameTimeNum_ << " minSameTime:" << minSameTime_.toFormattedString()<< " bidTime:" << cur.toFormattedString();
        }
      }
    }
    else
    {
      conn->shutdown();
    }
  }

  void halfSecondsOut()
  {
    LOG_INFO << "half timeout";
    LOG_INFO << "startTime:" << start_.toFormattedString()
        << " endTime:" << end_.toFormattedString()
        << " halfSeconds:" << halfSeconds_
        << " maxLicenses:" << maxLicenses_
        << " alertPrice:" << alertPrice_
        << " minPrice:" << minPrice_
        << " minSameTimeNum:" << minSameTimeNum_
        << " minSameTime:" << minSameTime_.toFormattedString()
        << " bidders:" << sessions_.size();
  }

  void timeout()
  {
    LOG_INFO << "timeout";

    everySecond();

    {
      MutexLockGuard lock(mutex_);
      ConnectionMap::iterator it = connections_.begin();
      for ( ; it != connections_.end(); ++it)
        it->second->shutdown();
    }

    LOG_INFO << "startTime:" << start_.toFormattedString()
        << " endTime:" << end_.toFormattedString()
        << " halfSeconds:" << halfSeconds_
        << " maxLicenses:" << maxLicenses_
        << " alertPrice:" << alertPrice_
        << " minPrice:" << minPrice_
        << " minSameTimeNum:" << minSameTimeNum_
        << " minSameTime:" << minSameTime_.toFormattedString()
        << " bidders:" << sessions_.size();
  }

  void sendBidStatus(const TcpConnectionPtr& conn)
  {
    loop_->assertInLoopThread();

    licenseplate::BidStatus msg;
    msg.set_maxlicenses(maxLicenses_);
    msg.set_alertprice(alertPrice_);
    msg.set_starttime(start_.secondsSinceEpoch());
    msg.set_lasttime(halfSeconds_*2);
    time_t ts = Timestamp::now().secondsSinceEpoch();
    msg.set_curtime(ts);
    msg.set_bidders(static_cast<uint32_t>(sessions_.size()));
    msg.set_minsuccessprice(minPrice_);
    msg.set_lastsuccesstime(minSameTime_.secondsSinceEpoch());
    msg.set_samesuccesstimenum(minSameTimeNum_);

    if (!conn->getContext().empty())
    {
      uint32_t id = boost::any_cast<uint32_t>(conn->getContext());
      SessionMap::iterator sessionIter = sessions_.find(id);
      if (sessionIter != sessions_.end())
      {
        std::vector<Session::SessionData>& vec = sessionIter->second.datas_;
        for (size_t i=0; i<vec.size(); ++i)
        {
          licenseplate::BidStatus::YetBidData* data = msg.add_yetbiddata();
          data->set_bidtime(vec[i].ts.secondsSinceEpoch());
          data->set_price(vec[i].price);
        }
      }
    }
    codec_.send(conn, msg);
  }

  void everySecond()
  {
    LOG_INFO << "everySecond";

    MutexLockGuard lock(mutex_);
    ConnectionMap::iterator it = connections_.begin();
    for ( ; it != connections_.end(); ++it)
    {
      sendBidStatus(it->second);
    }
  }

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  MutexLock mutex_;
  typedef std::map<muduo::string, muduo::net::TcpConnectionPtr> ConnectionMap;
  ConnectionMap connections_;

  struct BidData
  {
    uint32_t id;
    uint32_t price;
    Timestamp ts;
  };
  struct BidGreator : public std::binary_function<const BidData&, const BidData&, bool>
  {
    bool operator() (const BidData& lhs, const BidData& rhs) const
    {
      if (lhs.id == rhs.id) return false;

      if (lhs.price > rhs.price) return true;
      else if (lhs.price == rhs.price && lhs.ts < rhs.ts) return true;
      else if (lhs.price == rhs.price && lhs.ts == rhs.ts
               && lhs.id < rhs.id) return true;
      else
        return false;
    }
  };
  typedef std::set<BidData, BidGreator> BidSet;
  BidSet bids_;
  uint32_t halfSeconds_;
  uint32_t maxLicenses_;
  uint32_t alertPrice_;
  uint32_t minPrice_;
  uint32_t minSameTimeNum_;
  Timestamp minSameTime_;

  typedef std::map<uint32_t, Session> SessionMap;
  SessionMap sessions_;
  Timestamp start_;
  Timestamp half_;
  Timestamp end_;
};

int main(int argc, char* argv[])
{
  muduo::setLoggerOutputFile("licenseplateserver", 500*1000*1000, true, 1, 1);
  muduo::Logger::setLogLevel(Logger::INFO);
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

  Lua luaMgr;
  luaMgr.loadDir("./luaconfig");
  const char* ip = luaMgr.get<const char*>("LICENSE_PLATE_SERVER_IP");
  uint32_t port = luaMgr.get<uint32_t>("LICENSE_PLATE_SERVER_PORT");
  uint32_t threadCount = luaMgr.get<uint32_t>("LICENSE_PLATE_SERVER_THREADS");
  uint32_t halfSeconds = luaMgr.get<uint32_t>("LICENSE_PLATE_HALFSECONDS");
  uint32_t maxLicenses = luaMgr.get<uint32_t>("LICENSE_PLATE_MAXLICENSES");
  uint32_t alertPrice = luaMgr.get<uint32_t>("LICENSE_PLATE_ALERTPRICE");
  InetAddress listenAddr(ip, port);

  EventLoop loop;
  LicensePlateServer server(&loop, listenAddr, threadCount, halfSeconds, maxLicenses, alertPrice);

  server.start();
  loop.loop();
}
