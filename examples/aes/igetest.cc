#include <muduo/base/Aes.h>
#include <string.h>
#include <iostream>
using namespace muduo;

int main()
{
  Aes aes;

  const unsigned char in[32] =
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* in */

  const unsigned char out[32] =
  { 0x1a, 0x85, 0x19, 0xa6, 0x55, 0x7b, 0xe6, 0x52,
    0xe9, 0xda, 0x8e, 0x43, 0xda, 0x4e, 0xf4, 0x45,
    0x3c, 0xf4, 0x56, 0xb4, 0xca, 0x48, 0x8a, 0xa3,
    0x83, 0xc7, 0x9c, 0x98, 0xb3, 0x47, 0x97, 0xcb }; /* out */


  unsigned char encrypted[32] = {0};
  unsigned char decrypted[32] = {0};
  aes.encrypt(in, encrypted, 32);
  if (memcmp(encrypted, out, 32))
  {
    std::cout<<"encrypted != out"<<std::endl;
  }


  aes.decrypt(encrypted, decrypted, 32);
  if (memcmp(decrypted, in, 32))
  {
    std::cout<<"decrypted != in"<<std::endl;
  }

  bzero(encrypted, 32);
  bzero(decrypted, 32);
  aes.encrypt(in, encrypted, 32);
  if (memcmp(encrypted, out, 32))
  {
    std::cout<<"encrypted2 != out"<<std::endl;
  }


  aes.decrypt(encrypted, decrypted, 32);
  if (memcmp(decrypted, in, 32))
  {
    std::cout<<"decrypted2 != in"<<std::endl;
  }

  bzero(encrypted, 32);
  bzero(decrypted, 32);
  aes.encrypt(in, encrypted, 16);
  aes.encrypt(in+16, encrypted+16, 16);
  if (memcmp(encrypted, out, 32))
  {
    std::cout<<"encrypted 16 != out"<<std::endl;
  }
  aes.decrypt(encrypted, decrypted, 16);
  aes.decrypt(encrypted+16, decrypted+16, 16);
  if (memcmp(decrypted, in, 32))
  {
    std::cout<<"decrypted 16 != in"<<std::endl;
  }

  bzero(encrypted, 32);
  bzero(decrypted, 32);
  aes.encrypt(out, encrypted, 32);
  aes.decrypt(encrypted, decrypted, 32);
  if (memcmp(decrypted, out, 32))
  {
    std::cout<<"decrypted3 != out"<<std::endl;
  }

  return 0;
}
