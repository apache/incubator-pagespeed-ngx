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

#include "net/instaweb/util/public/simple_stats.h"

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

SimpleStatsVariable::~SimpleStatsVariable() {
}

SimpleStats::SimpleStats() {
  // TODO(jmarantz): provide an alternate constructor to pass in the thread
  // system.
  thread_system_.reset(ThreadSystem::CreateThreadSystem());
}

SimpleStats::~SimpleStats() {
}

SimpleStatsVariable* SimpleStats::NewVariable(
    const StringPiece& name, int index) {
  return new SimpleStatsVariable(thread_system_->NewMutex());
}

SimpleStatsVariable::SimpleStatsVariable(AbstractMutex* mutex)
    : value_(0),
      mutex_(mutex) {
}

int64 SimpleStatsVariable::Get64() const {
  ScopedMutex lock(mutex_.get());
  return value_;
}

void SimpleStatsVariable::Set64(int64 value) {
  ScopedMutex lock(mutex_.get());
  value_ = value;
}

int64 SimpleStatsVariable::Add(int delta) {
  ScopedMutex lock(mutex_.get());
  value_ += delta;
  return value_;
}

}  // namespace net_instaweb
