// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/util/public/shared_mem_statistics.h"

#include <cstddef>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kStatisticsObjName[] = "statistics";

}  // namespace

namespace net_instaweb {

// Our shared memory storage format is an array of (mutex, int64).

SharedMemVariable::SharedMemVariable(const StringPiece& name)
    : name_(name.as_string()),
      value_ptr_(NULL) {
}

int64 SharedMemVariable::Get64() const {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    return *value_ptr_;
  } else {
    return -1;
  }
}

int SharedMemVariable::Get() const {
  return Get64();
}

void SharedMemVariable::Set(int new_value) {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    *value_ptr_ = new_value;
  }
}

void SharedMemVariable::Add(int delta) {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    *value_ptr_ += delta;
  }
}

void SharedMemVariable::AttachTo(
    AbstractSharedMemSegment* segment, size_t offset,
    MessageHandler* message_handler) {
  mutex_.reset(segment->AttachToSharedMutex(offset));
  if (mutex_.get() == NULL) {
    message_handler->Message(
        kError, "Unable to attach to mutex for statistics variable %s",
        name_.c_str());
  }

  value_ptr_ = reinterpret_cast<volatile int64*>(
      segment->Base() + offset + segment->SharedMutexSize());
}

void SharedMemVariable::Reset() {
  mutex_.reset();
}

SharedMemStatistics::SharedMemStatistics(AbstractSharedMem* shm_runtime,
                                         const GoogleString& filename_prefix)
    : shm_runtime_(shm_runtime), filename_prefix_(filename_prefix),
      frozen_(false) {
}

SharedMemStatistics::~SharedMemStatistics() {
}

SharedMemVariable* SharedMemStatistics::NewVariable(const StringPiece& name,
                                                    int index) {
  if (frozen_) {
    LOG(ERROR) << "Cannot add variable " << name
               << " after SharedMemStatistics is frozen!";
    return NULL;
  } else {
    return new SharedMemVariable(name);
  }
}

bool SharedMemStatistics::InitMutexes(size_t per_var,
                                      MessageHandler* message_handler) {
  for (size_t i = 0; i < variables_.size(); ++i) {
    SharedMemVariable* var = variables_[i];
    if (!segment_->InitializeSharedMutex(i * per_var, message_handler)) {
      message_handler->Message(
          kError, "Unable to create mutex for statistics variable %s",
          var->name_.c_str());
      return false;
    }
  }
  return true;
}

void SharedMemStatistics::InitVariables(bool parent,
                                        MessageHandler* message_handler) {
  frozen_ = true;

  // Compute size of shared memory
  size_t per_var = shm_runtime_->SharedMutexSize() +
                   sizeof(int64);  // NOLINT(runtime/sizeof)
  size_t total = variables_.size() * per_var;

  bool ok = true;
  if (parent) {
    // In root process -> initialize shared memory.
    segment_.reset(
        shm_runtime_->CreateSegment(SegmentName(), total, message_handler));
    ok = (segment_.get() != NULL);

    // Init the locks
    if (ok) {
      if (!InitMutexes(per_var, message_handler)) {
        // We had a segment but could not make some mutex. In this case,
        // we can't predict what would happen if the child process tried
        // to touch messed up mutexes. Accordingly, we blow away the
        // segment.
        segment_.reset(NULL);
        shm_runtime_->DestroySegment(SegmentName(), message_handler);
      }
    }
  } else {
    // Child -> attach to existing segment
    segment_.reset(
        shm_runtime_->AttachToSegment(SegmentName(), total, message_handler));
    ok = (segment_.get() != NULL);
  }

  // Now make the variable objects actually point to the right things.
  for (size_t i = 0; i < variables_.size(); ++i) {
    if (ok) {
      variables_[i]->AttachTo(segment_.get(), i * per_var, message_handler);
    } else {
      variables_[i]->Reset();
    }
  }
}

void SharedMemStatistics::GlobalCleanup(MessageHandler* message_handler) {
  if (segment_.get() != NULL) {
    shm_runtime_->DestroySegment(SegmentName(), message_handler);
  }
}

GoogleString SharedMemStatistics::SegmentName() const {
  return StrCat(filename_prefix_, kStatisticsObjName);
}

}  // namespace net_instaweb
