/*
 * Copyright 2015 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)

# define __WINDOWS__
# include <winsock2.h>
#else
# include <arpa/inet.h>
# include <sys/types.h>
#endif

#define BSON_BIG_ENDIAN    4321
#define BSON_LITTLE_ENDIAN 1234

#if defined(__sun)
# include <sys/byteorder.h>
# if defined(_LITTLE_ENDIAN)
#  define BSON_BYTE_ORDER 1234
# else
#  define BSON_BYTE_ORDER 4321
# endif
#endif

#ifndef BSON_BYTE_ORDER
# if BYTE_ORDER == LITTLE_ENDIAN
#  define BSON_BYTE_ORDER 1234
# elif BYTE_ORDER == BIG_ENDIAN
#  define BSON_BYTE_ORDER 4321
# endif
#endif

#if defined(__sun)
# define BSON_UINT32_SWAP_LE_BE(v) BSWAP_32((uint32_t)v)
# define BSON_UINT64_SWAP_LE_BE(v) BSWAP_64((uint64_t)v)
#elif defined(__clang__) && defined(__clang_major__) && defined(__clang_minor__) && \
  (__clang_major__ >= 3) && (__clang_minor__ >= 1)
# if __has_builtin(__builtin_bswap32)
#  define BSON_UINT32_SWAP_LE_BE(v) __builtin_bswap32(v)
# endif
# if __has_builtin(__builtin_bswap64)
#  define BSON_UINT64_SWAP_LE_BE(v) __builtin_bswap64(v)
# endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
# if __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 3
#  define BSON_UINT32_SWAP_LE_BE(v) __builtin_bswap32 ((uint32_t)v)
#  define BSON_UINT64_SWAP_LE_BE(v) __builtin_bswap64 ((uint64_t)v)
# endif
#endif

#ifndef BSON_UINT32_SWAP_LE_BE
# define BSON_UINT32_SWAP_LE_BE(v) __bson_uint32_swap_slow ((uint32_t)v)
#endif

#ifndef BSON_UINT64_SWAP_LE_BE
# define BSON_UINT64_SWAP_LE_BE(v) __bson_uint64_swap_slow ((uint64_t)v)
#endif

#if BSON_BYTE_ORDER == BSON_LITTLE_ENDIAN
# define BSON_UINT32_FROM_LE(v)  ((uint32_t)v)
# define BSON_UINT32_TO_LE(v)    ((uint32_t)v)
# define BSON_UINT32_FROM_BE(v)  BSON_UINT32_SWAP_LE_BE(v)
# define BSON_UINT32_TO_BE(v)    BSON_UINT32_SWAP_LE_BE(v)
# define BSON_UINT64_FROM_LE(v)  ((uint64_t)v)
# define BSON_UINT64_TO_LE(v)    ((uint64_t)v)
# define BSON_UINT64_FROM_BE(v)  BSON_UINT64_SWAP_LE_BE(v)
# define BSON_UINT64_TO_BE(v)    BSON_UINT64_SWAP_LE_BE(v)
# define BSON_DOUBLE_FROM_LE(v)  ((double)v)
# define BSON_DOUBLE_TO_LE(v)    ((double)v)
#elif BSON_BYTE_ORDER == BSON_BIG_ENDIAN
# define BSON_UINT32_FROM_LE(v)  BSON_UINT32_SWAP_LE_BE(v)
# define BSON_UINT32_TO_LE(v)    BSON_UINT32_SWAP_LE_BE(v)
# define BSON_UINT32_FROM_BE(v)  ((uint32_t)v)
# define BSON_UINT32_TO_BE(v)    ((uint32_t)v)
# define BSON_UINT64_FROM_LE(v)  BSON_UINT64_SWAP_LE_BE(v)
# define BSON_UINT64_TO_LE(v)    BSON_UINT64_SWAP_LE_BE(v)
# define BSON_UINT64_FROM_BE(v)  ((uint64_t)v)
# define BSON_UINT64_TO_BE(v)    ((uint64_t)v)
# define BSON_DOUBLE_FROM_LE(v)  (__bson_double_swap_slow(v))
# define BSON_DOUBLE_TO_LE(v)    (__bson_double_swap_slow(v))
#else
# error "The endianness of target architecture is unknown."
#endif

#if SIZEOF_LONG == 8
#define NUM2INT64(v) NUM2LONG(v)
#define INT642NUM(v) LONG2NUM(v)
#else
#define NUM2INT64(v) NUM2LL(v)
#define INT642NUM(v) LL2NUM(v)
#endif

/*
 *--------------------------------------------------------------------------
 *
 * __bson_uint32_swap_slow --
 *
 *       Fallback endianness conversion for 32-bit integers.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static uint32_t __bson_uint32_swap_slow(uint32_t v)
{
   return ((v & 0x000000FFU) << 24) |
          ((v & 0x0000FF00U) <<  8) |
          ((v & 0x00FF0000U) >>  8) |
          ((v & 0xFF000000U) >> 24);
}


/*
 *--------------------------------------------------------------------------
 *
 * __bson_uint64_swap_slow --
 *
 *       Fallback endianness conversion for 64-bit integers.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static uint64_t __bson_uint64_swap_slow (uint64_t v)
{
   return ((v & 0x00000000000000FFULL) << 56) |
          ((v & 0x000000000000FF00ULL) << 40) |
          ((v & 0x0000000000FF0000ULL) << 24) |
          ((v & 0x00000000FF000000ULL) <<  8) |
          ((v & 0x000000FF00000000ULL) >>  8) |
          ((v & 0x0000FF0000000000ULL) >> 24) |
          ((v & 0x00FF000000000000ULL) >> 40) |
          ((v & 0xFF00000000000000ULL) >> 56);
}

/*
 *--------------------------------------------------------------------------
 *
 * __bson_double_swap_slow --
 *
 *       Fallback endianness conversion for double floating point.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static double __bson_double_swap_slow(double v)
{
   uint64_t uv;

   memcpy(&uv, &v, sizeof(v));
   uv = BSON_UINT64_SWAP_LE_BE(uv);
   memcpy(&v, &uv, sizeof(v));

   return v;
}

