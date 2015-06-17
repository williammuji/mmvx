#include <muduo/base/Aes.h>
#include <string.h>

using namespace muduo;

static const unsigned char aesKey[16] = 
{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static unsigned char aesIv[AES_BLOCK_SIZE*4] = 
{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };

Aes::Aes()
{
  AES_set_encrypt_key(aesKey, 8*sizeof aesKey, &encryptKey_);
  AES_set_decrypt_key(aesKey, 8*sizeof aesKey, &decryptKey_);
}

Aes::~Aes()
{
}

void Aes::encrypt(const unsigned char* in, unsigned char* out, size_t length)
{
  unsigned char iv[AES_BLOCK_SIZE*4];
  memcpy(iv, aesIv, sizeof iv);
  AES_ige_encrypt(in, out, length, &encryptKey_, iv, AES_ENCRYPT);
}

void Aes::decrypt(const unsigned char* in, unsigned char* out, size_t length)
{
  unsigned char iv[AES_BLOCK_SIZE*4];
  memcpy(iv, aesIv, sizeof iv);
  AES_ige_encrypt(in, out, length, &decryptKey_, iv, AES_DECRYPT);
}
