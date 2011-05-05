/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)

// Compile-time only checks of various properties that don't fit anywhere else.
//
// For now this just makes sure that some things are constants so they can be
// used in initializers safely.

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {
namespace {

// The COMPILE_ASSERTs and enum MustBeConstants are both testing the same thing.
// That Timer::k... are compile-time constants. We include both tests because
// there is enough magic to warrant two different approaches.
COMPILE_ASSERT(Timer::kSecondMs == 1000, seconds_literal);
COMPILE_ASSERT(Timer::kSecondUs == 1000 * 1000, seconds_us_literal);
COMPILE_ASSERT(Timer::kSecondNs == 1000 * 1000 * 1000, seconds_ns_literal);
COMPILE_ASSERT(Timer::kMinuteMs == 60 * 1000, minutes_literal);
COMPILE_ASSERT(Timer::kHourMs   == 60 * 60 * 1000, hours_literal);

// enumerators can only be initialized to compile-time constants, so this
// would not build if any of these weren't compile-time defined.
enum MustBeConstants {
  kSecondMs = Timer::kSecondMs,
  kSecondUs = Timer::kSecondUs,
  kSecondNs = Timer::kSecondNs,
  kMinuteMs = Timer::kMinuteMs,
  kHourMs   = Timer::kHourMs,
  kDayMs    = Timer::kDayMs,
  kWeekMs   = Timer::kWeekMs,
  kMonthMs  = Timer::kMonthMs,
  kYearMs   = Timer::kYearMs,
  kImplicitCacheTtlMs = ResponseHeaders::kImplicitCacheTtlMs
};

}  // namespace
}  // namespace net_instaweb
