// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation for compiler-based
// ThreadSanitizer. Use base/atomicops.h instead.

#ifndef BASE_ATOMICOPS_INTERNALS_TSAN_H_
#define BASE_ATOMICOPS_INTERNALS_TSAN_H_

#include "base/base_export.h"

// This struct is not part of the public API of this module; clients may not
// use it.  (However, it's exported via BASE_EXPORT because clients implicitly
// do use it at link time by inlining these functions.)
// Features of this x86.  Values may not be correct before main() is run,
// but are set conservatively.
struct AtomicOps_x86CPUFeatureStruct {
  bool has_amd_lock_mb_bug;  // Processor has AMD memory-barrier bug; do lfence
                             // after acquire compare-and-swap.
  bool has_sse2;             // Processor has SSE2.
};
BASE_EXPORT extern struct AtomicOps_x86CPUFeatureStruct
    AtomicOps_Internalx86CPUFeatures;

#define ATOMICOPS_COMPILER_BARRIER() __asm__ __volatile__("" : : : "memory")

namespace base {
namespace subtle {

#ifndef TSAN_INTERFACE_ATOMIC_H
#define TSAN_INTERFACE_ATOMIC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char  __tsan_atomic8;
typedef short __tsan_atomic16;  // NOLINT
typedef int   __tsan_atomic32;
typedef long  __tsan_atomic64;  // NOLINT

typedef enum {
  __tsan_memory_order_relaxed = 1 << 0,
  __tsan_memory_order_consume = 1 << 1,
  __tsan_memory_order_acquire = 1 << 2,
  __tsan_memory_order_release = 1 << 3,
  __tsan_memory_order_acq_rel = 1 << 4,
  __tsan_memory_order_seq_cst = 1 << 5,
} __tsan_memory_order;

__tsan_atomic8 __tsan_atomic8_load(const volatile __tsan_atomic8 *a,
    __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_load(const volatile __tsan_atomic16 *a,
    __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_load(const volatile __tsan_atomic32 *a,
    __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_load(const volatile __tsan_atomic64 *a,
    __tsan_memory_order mo);

void __tsan_atomic8_store(volatile __tsan_atomic8 *a, __tsan_atomic8 v,
    __tsan_memory_order mo);
void __tsan_atomic16_store(volatile __tsan_atomic16 *a, __tsan_atomic16 v,
    __tsan_memory_order mo);
void __tsan_atomic32_store(volatile __tsan_atomic32 *a, __tsan_atomic32 v,
    __tsan_memory_order mo);
void __tsan_atomic64_store(volatile __tsan_atomic64 *a, __tsan_atomic64 v,
    __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_exchange(volatile __tsan_atomic8 *a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_exchange(volatile __tsan_atomic16 *a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_exchange(volatile __tsan_atomic32 *a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_exchange(volatile __tsan_atomic64 *a,
    __tsan_atomic64 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_add(volatile __tsan_atomic8 *a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_add(volatile __tsan_atomic16 *a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_add(volatile __tsan_atomic32 *a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_add(volatile __tsan_atomic64 *a,
    __tsan_atomic64 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_and(volatile __tsan_atomic8 *a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_and(volatile __tsan_atomic16 *a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_and(volatile __tsan_atomic32 *a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_and(volatile __tsan_atomic64 *a,
    __tsan_atomic64 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_or(volatile __tsan_atomic8 *a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_or(volatile __tsan_atomic16 *a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_or(volatile __tsan_atomic32 *a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_or(volatile __tsan_atomic64 *a,
    __tsan_atomic64 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_xor(volatile __tsan_atomic8 *a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_xor(volatile __tsan_atomic16 *a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_xor(volatile __tsan_atomic32 *a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_xor(volatile __tsan_atomic64 *a,
    __tsan_atomic64 v, __tsan_memory_order mo);

int __tsan_atomic8_compare_exchange_weak(volatile __tsan_atomic8 *a,
    __tsan_atomic8 *c, __tsan_atomic8 v, __tsan_memory_order mo);
int __tsan_atomic16_compare_exchange_weak(volatile __tsan_atomic16 *a,
    __tsan_atomic16 *c, __tsan_atomic16 v, __tsan_memory_order mo);
int __tsan_atomic32_compare_exchange_weak(volatile __tsan_atomic32 *a,
    __tsan_atomic32 *c, __tsan_atomic32 v, __tsan_memory_order mo);
int __tsan_atomic64_compare_exchange_weak(volatile __tsan_atomic64 *a,
    __tsan_atomic64 *c, __tsan_atomic64 v, __tsan_memory_order mo);

int __tsan_atomic8_compare_exchange_strong(volatile __tsan_atomic8 *a,
    __tsan_atomic8 *c, __tsan_atomic8 v, __tsan_memory_order mo);
int __tsan_atomic16_compare_exchange_strong(volatile __tsan_atomic16 *a,
    __tsan_atomic16 *c, __tsan_atomic16 v, __tsan_memory_order mo);
int __tsan_atomic32_compare_exchange_strong(volatile __tsan_atomic32 *a,
    __tsan_atomic32 *c, __tsan_atomic32 v, __tsan_memory_order mo);
int __tsan_atomic64_compare_exchange_strong(volatile __tsan_atomic64 *a,
    __tsan_atomic64 *c, __tsan_atomic64 v, __tsan_memory_order mo);

void __tsan_atomic_thread_fence(__tsan_memory_order mo);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // #ifndef TSAN_INTERFACE_ATOMIC_H

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32 *ptr,
                                  Atomic32 old_value,
                                  Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
    __tsan_memory_order_relaxed);
  return cmp;
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32 *ptr,
                                  Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_relaxed);
}

inline Atomic32 Acquire_AtomicExchange(volatile Atomic32 *ptr,
                                Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_acquire);
}

inline Atomic32 Release_AtomicExchange(volatile Atomic32 *ptr,
                                Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_release);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32 *ptr,
                                   Atomic32 increment) {
  return increment + __tsan_atomic32_fetch_add(ptr, increment,
      __tsan_memory_order_relaxed);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32 *ptr,
                                 Atomic32 increment) {
  return increment + __tsan_atomic32_fetch_add(ptr, increment,
      __tsan_memory_order_acq_rel);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32 *ptr,
                                Atomic32 old_value,
                                Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_acquire);
  return cmp;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32 *ptr,
                                Atomic32 old_value,
                                Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_release);
  return cmp;
}

inline void NoBarrier_Store(volatile Atomic32 *ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_relaxed);
}

inline void Acquire_Store(volatile Atomic32 *ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_relaxed);
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

inline void Release_Store(volatile Atomic32 *ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_release);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32 *ptr) {
  return __tsan_atomic32_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic32 Acquire_Load(volatile const Atomic32 *ptr) {
  return __tsan_atomic32_load(ptr, __tsan_memory_order_acquire);
}

inline Atomic32 Release_Load(volatile const Atomic32 *ptr) {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
  return __tsan_atomic32_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64 *ptr,
                                      Atomic64 old_value,
                                      Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_relaxed);
  return cmp;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64 *ptr,
                                      Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_AtomicExchange(volatile Atomic64 *ptr,
                                    Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_acquire);
}

inline Atomic64 Release_AtomicExchange(volatile Atomic64 *ptr,
                                    Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_release);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64 *ptr,
                                       Atomic64 increment) {
  return increment + __tsan_atomic64_fetch_add(ptr, increment,
      __tsan_memory_order_relaxed);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64 *ptr,
                                     Atomic64 increment) {
  return increment + __tsan_atomic64_fetch_add(ptr, increment,
      __tsan_memory_order_acq_rel);
}

inline void NoBarrier_Store(volatile Atomic64 *ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_relaxed);
}

inline void Acquire_Store(volatile Atomic64 *ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_relaxed);
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

inline void Release_Store(volatile Atomic64 *ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_release);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64 *ptr) {
  return __tsan_atomic64_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_Load(volatile const Atomic64 *ptr) {
  return __tsan_atomic64_load(ptr, __tsan_memory_order_acquire);
}

inline Atomic64 Release_Load(volatile const Atomic64 *ptr) {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
  return __tsan_atomic64_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64 *ptr,
                                    Atomic64 old_value,
                                    Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_acquire);
  return cmp;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64 *ptr,
                                    Atomic64 old_value,
                                    Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_release);
  return cmp;
}

inline void MemoryBarrier() {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

}  // namespace base::subtle
}  // namespace base

#undef ATOMICOPS_COMPILER_BARRIER

#endif  // BASE_ATOMICOPS_INTERNALS_TSAN_H_
