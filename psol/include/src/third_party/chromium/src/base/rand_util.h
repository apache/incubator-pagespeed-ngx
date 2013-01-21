// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"

namespace base {

// Returns a random number in range [0, kuint64max]. Thread-safe.
BASE_API uint64 RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
BASE_API int RandInt(int min, int max);

// Returns a random number in range [0, max).  Thread-safe.
//
// Note that this can be used as an adapter for std::random_shuffle():
// Given a pre-populated |std::vector<int> myvector|, shuffle it as
//   std::random_shuffle(myvector.begin(), myvector.end(), base::RandGenerator);
BASE_API uint64 RandGenerator(uint64 max);

// Returns a random double in range [0, 1). Thread-safe.
BASE_API double RandDouble();

// Given input |bits|, convert with maximum precision to a double in
// the range [0, 1). Thread-safe.
BASE_API double BitsToOpenEndedUnitInterval(uint64 bits);

// Fills |output_length| bytes of |output| with cryptographically strong random
// data.
BASE_API void RandBytes(void* output, size_t output_length);

// Fills a string of length |length| with with cryptographically strong random
// data and returns it.
//
// Not that this is a variation of |RandBytes| with a different return type.
BASE_API std::string RandBytesAsString(size_t length);

}  // namespace base

#endif  // BASE_RAND_UTIL_H_
