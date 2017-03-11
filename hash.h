#ifndef HASH_H
#define HASH_H

#if defined(SHA1_BLK)
#include "block-sha1/sha1.h"
#elif defined(SHA1_PPC)
#include "ppc/sha1.h"
#elif defined(SHA1_APPLE)
#include <CommonCrypto/CommonDigest.h>
#else /* SHA1_OPENSSL */
#include <openssl/sha.h>
#endif

#endif
