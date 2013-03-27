// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines cross-platform ByteSwap() implementations for 16, 32 and
// 64-bit values, and NetToHostXX() / HostToNextXX() functions equivalent to
// the traditional ntohX() and htonX() functions.
// Use the functions defined here rather than using the platform-specific
// functions directly.

#ifndef BASE_SYS_BYTEORDER_H_
#define BASE_SYS_BYTEORDER_H_

#include "base/basictypes.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

// Include headers to provide byteswap for all platforms.
#if defined(COMPILER_MSVC)
#include <stdlib.h>
#elif defined(OS_MACOSX)
#include <libkern/OSByteOrder.h>
#elif defined(OS_OPENBSD)
#include <sys/endian.h>
#else
#include <byteswap.h>
#endif


namespace base {

// Returns a value with all bytes in |x| swapped, i.e. reverses the endianness.
inline uint16 ByteSwap(uint16 x) {
#if defined(COMPILER_MSVC)
  return _byteswap_ushort(x);
#elif defined(OS_MACOSX)
  return OSSwapInt16(x);
#elif defined(OS_OPENBSD)
  return swap16(x);
#else
  return bswap_16(x);
#endif
}
inline uint32 ByteSwap(uint32 x) {
#if defined(COMPILER_MSVC)
  return _byteswap_ulong(x);
#elif defined(OS_MACOSX)
  return OSSwapInt32(x);
#elif defined(OS_OPENBSD)
  return swap32(x);
#else
  return bswap_32(x);
#endif
}
inline uint64 ByteSwap(uint64 x) {
#if defined(COMPILER_MSVC)
  return _byteswap_uint64(x);
#elif defined(OS_MACOSX)
  return OSSwapInt64(x);
#elif defined(OS_OPENBSD)
  return swap64(x);
#else
  return bswap_64(x);
#endif
}

// Converts the bytes in |x| from host order (endianness) to little endian, and
// returns the result.
inline uint16 ByteSwapToLE16(uint16 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint32 ByteSwapToLE32(uint32 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint64 ByteSwapToLE64(uint64 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}

// Converts the bytes in |x| from network to host order (endianness), and
// returns the result.
inline uint16 NetToHost16(uint16 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint32 NetToHost32(uint32 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint64 NetToHost64(uint64 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

// Converts the bytes in |x| from host to network order (endianness), and
// returns the result.
inline uint16 HostToNet16(uint16 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint32 HostToNet32(uint32 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint64 HostToNet64(uint64 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

}  // namespace base


#endif  // BASE_SYS_BYTEORDER_H_
