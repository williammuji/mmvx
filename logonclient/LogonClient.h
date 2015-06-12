#ifndef LOGON_CLIENT_H
#define LOGON_CLIENT_H

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <muduo/net/protobuf/ProtobufDispatcher.h>
#include <muduo/net/protobuf/ProtobufCodec.h>
#include <proto/logon.pb.h>

#include <muduo/base/Types.h>
typedef boost::shared_ptr<muduo::LogonRet> LogonRetPtr;

class LogonClientPool;
class ForwardClient;

class LogonClient : boost::noncopyable
{
 public:
  LogonClient(muduo::net::EventLoop* loop,
              const muduo::net::InetAddress& serverAddr,
              const muduo::string& name,
              LogonClientPool* pool);

  void start();
  void stop();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr&,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  void onLogonRet(const muduo::net::TcpConnectionPtr&,
                  const LogonRetPtr& message,
                  muduo::Timestamp);


  muduo::net::TcpClient client_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  LogonClientPool* pool_;
  boost::scoped_ptr<ForwardClient> forwardClient_;
};

class LogonClientPool : boost::noncopyable
{
 public:
  LogonClientPool(muduo::net::EventLoop* loop,
                  const muduo::net::InetAddress& serverAddr,
                  int numClients,
                  int numThreads);

 private:
  void quit();

  muduo::net::EventLoop* loop_;
  muduo::net::EventLoopThreadPool threadPool_;
  int numClients_;
  boost::ptr_vector<LogonClient> logonClients_;
};

#endif
