#ifndef __LIBLTE_SSL_H__
#define __LIBLTE_SSL_H__

#ifdef HAVE_POLARSSL

#include "polarssl/sha256.h"
#include "polarssl/aes.h"

void sha256(const unsigned char *key, size_t keylen,
            const unsigned char *input, size_t ilen,
            unsigned char output[32], int is224 )
{
  sha256_hmac(key, keylen, input, ilen, output, is224);
}

#endif // HAVE_POLARSSL

#ifdef HAVE_MBEDTLS

#include "mbedtls/md.h"
#include "mbedtls/aes.h"

typedef mbedtls_aes_context aes_context;

#define AES_ENCRYPT     1
#define AES_DECRYPT     0

int aes_setkey_enc( aes_context *ctx, const unsigned char *key, unsigned int keysize )
{
  return mbedtls_aes_setkey_enc(ctx, key, keysize);
}

int aes_crypt_ecb( aes_context *ctx,
                    int mode,
                    const unsigned char input[16],
                    unsigned char output[16] )
{
  return mbedtls_aes_crypt_ecb(ctx, mode, input, output);
}

void sha256(const unsigned char *key, size_t keylen,
            const unsigned char *input, size_t ilen,
            unsigned char output[32], int is224 )
{
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  key, keylen,
                  input, ilen,
                  output );
}

#endif // HAVE_MBEDTLS

#endif // __LIBLTE_SSL_H__
