/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/util/simple_stats.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

SimpleStats::SimpleStats(ThreadSystem* thread_system)
    : thread_system_(thread_system) {
}

SimpleStats::~SimpleStats() {
}

CountHistogram* SimpleStats::NewHistogram(StringPiece /*name*/) {
  return new CountHistogram(thread_system_->NewMutex());
}

SimpleStats::Var* SimpleStats::NewVariable(StringPiece name) {
  Var* var = new Var(name, this);
  var->impl()->set_mutex(thread_system_->NewMutex());
  return var;
}

SimpleStats::UpDown* SimpleStats::NewUpDownCounter(StringPiece name) {
  UpDown* up_down = new UpDown(name, this);
  up_down->impl()->set_mutex(thread_system_->NewMutex());
  return up_down;
}

SimpleStatsVariable::SimpleStatsVariable(StringPiece name, Statistics* stats)
    : value_(0) {}

SimpleStatsVariable::~SimpleStatsVariable() {
}

int64 SimpleStatsVariable::GetLockHeld() const {
  return value_;
}

int64 SimpleStatsVariable::SetReturningPreviousValueLockHeld(int64 value) {
  int64 previous_value = value_;
  value_ = value;
  return previous_value;
}

}  // namespace net_instaweb
