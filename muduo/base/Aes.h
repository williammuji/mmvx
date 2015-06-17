#ifndef MUDUO_AES_H
#define MUDUOAES_H

#include <boost/noncopyable.hpp>
#include <openssl/aes.h>

namespace muduo
{
class Aes : boost::noncopyable
  {
   public:
    Aes();
    ~Aes();

    void encrypt(const unsigned char* in, unsigned char* out, size_t length);
    void decrypt(const unsigned char* in, unsigned char* out, size_t length);

   private:
    AES_KEY encryptKey_;
    AES_KEY decryptKey_;
  };
}

#endif
