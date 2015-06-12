#ifndef FORWARD_CLIENT_H
#define FORWARD_CLIENT_H

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

class ForwardClient : boost::noncopyable
{
 public:
  ForwardClient(muduo::net::EventLoop* loop,
              const muduo::net::InetAddress& serverAddr,
              const muduo::string& name,
              int64_t uid,
              uint32_t session_);

  void start();
  void stop();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onUnknownMessage(const muduo::net::TcpConnectionPtr&,
                        const muduo::net::MessagePtr& message,
                        muduo::Timestamp);

  muduo::net::TcpClient client_;
  muduo::net::ProtobufDispatcher dispatcher_;
  muduo::net::ProtobufCodec codec_;
  int64_t uid_;
  uint32_t session_;
};

#endif
