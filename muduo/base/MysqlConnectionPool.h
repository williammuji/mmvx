#ifndef MYSQL_CONNECTION_POOL_H
#define MYSQL_CONNECTION_POOL_H

#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <mysql++.h>
#pragma GCC diagnostic warning "-Wfloat-conversion"
#pragma GCC diagnostic warning "-Wconversion"
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Thread.h>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <stdio.h>
#include <muduo/base/Logging.h>
#include <muduo/base/ThreadLocal.h>
namespace muduo
{

class MysqlConnectionPool : public mysqlpp::ConnectionPool
{
 public:
  typedef boost::function<void ()> Task;

  MysqlConnectionPool(const string& db,
                      const string& server,
                      const string& user,
                      const string& password,
                      int numThreads)
      : db_(db),
      server_(server),
      user_(user),
      password_(password),
      inUseConns_(0),
      threads_(numThreads)
  {
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32] = {0};
      snprintf(name, sizeof name, "MysqlConnectionThread %d", i);
      threads_.push_back(new muduo::Thread(
              boost::bind(&MysqlConnectionPool::threadFunc, this), muduo::string(name)));
    }
    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::start, _1));
  }

  ~MysqlConnectionPool()
  {
    clear();
    joinAll();
  }

  void put(const Task& task)
  {
    queue_.put(task);
  }

  void joinAll()
  {
    for (size_t i = 0; i < threads_.size(); ++i)
    {
      queue_.put(boost::bind(&MysqlConnectionPool::stop, this));
    }
    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::join, _1));
  }

  mysqlpp::Connection* grab()
  {
    while (inUseConns_ > kMaxInUseConns_) {
      sleep(1);
    }

    ++inUseConns_;
    return mysqlpp::ConnectionPool::grab();
  }

  void release(const mysqlpp::Connection* pc)
  {
    mysqlpp::ConnectionPool::release(pc);
    --inUseConns_;
  }

 protected:
  mysqlpp::Connection* create()
  {
    return new mysqlpp::Connection(
        db_.empty() ? 0 : db_.c_str(),
        server_.empty() ? 0 : server_.c_str(),
        user_.empty() ? 0 : user_.c_str(),
        password_.empty() ? "" : password_.c_str());
  }

  void destroy(mysqlpp::Connection* cp)
  {
    delete cp;
  }

  unsigned int max_idle_time()
  {
    return kMaxIdleTime_;
  }

  void stop()
  {
    bool& running = running_.value();
    running = false;
  }

 private:
  string db_, server_, user_, password_;
  unsigned int inUseConns_;
  static const unsigned int kMaxInUseConns_ = 8;
  static const unsigned int kMaxIdleTime_ = 3;

  muduo::BlockingQueue<Task> queue_;
  boost::ptr_vector<muduo::Thread> threads_;
  ThreadLocal<bool> running_;

  void threadFunc()
  {
    mysqlpp::Connection::thread_start();
    LOG_INFO << "tid=" << muduo::CurrentThread::tid() << ", " << muduo::CurrentThread::name() << " started";

    bool& running = running_.value();
    running = true;
    while (running)
    {
      queue_.take()();
    }

     mysqlpp::Connection::thread_end();
    LOG_INFO << "tid=" << muduo::CurrentThread::tid() << ", stopped" << muduo::CurrentThread::name();
  }

};

}

#endif
