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

#include "net/instaweb/apache/apr_statistics.h"

#include "apr_global_mutex.h"
#include "apr_shm.h"
#include "apr_strings.h"
#include "apr.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "apr_time.h"
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/stack_buffer.h"

extern "C" {
#include "httpd.h"
#include "http_config.h"
#ifdef AP_NEED_SET_MUTEX_PERMS
#include "unixd.h"
#endif
}


namespace {

const char kStatisticsDir[] = "statistics";
const char kStatisticsMutexPrefix[] = "statistics/stats_mutex.";
const char kStatisticsValuePrefix[] = "statistics/stats_value.";

}  // namespace

namespace net_instaweb {

#define COUNT_LOCK_WAIT_TIME 0
#if COUNT_LOCK_WAIT_TIME
// Unlocked counter, which is reasonably safe on x86 if it's 32-bit, which
// is good for over hour of of locked time, which is good enough for
// experiments, particularly on a prefork system where the only extra
// thread is the one from serf.
static uint32 accumulated_time_in_global_locks_us = 0;
static uint32 prev_message_us = 0;
#endif

// Helper class for lexically scoped mutexing.
// TODO(jmarantz): consier merging this with ScopedLock.
class AprScopedGlobalLock {
 public:
  explicit AprScopedGlobalLock(const AprVariable* var) : variable_(var) {
    if (variable_->mutex_ == NULL) {
      acquired_ = false;
    } else {
#if COUNT_LOCK_WAIT_TIME
      int64 time_us = apr_time_now();
#endif
      acquired_ = variable_->CheckResult(
          apr_global_mutex_lock(variable_->mutex_), "lock mutex");
#if COUNT_LOCK_WAIT_TIME
      int64 delta_us = apr_time_now() - time_us;
      if (delta_us > 0) {
        accumulated_time_in_global_locks_us += delta_us;
        if ((accumulated_time_in_global_locks_us - prev_message_us) > 1000000) {
          // race condition is possible here
          prev_message_us = accumulated_time_in_global_locks_us;
          double time_wasted_seconds = accumulated_time_in_global_locks_us
              / 1000000.0;
          LOG(ERROR) << "Cumulative locked time spent: " << time_wasted_seconds
                     << "seconds";
        }
      }
#endif
    }
  }

  ~AprScopedGlobalLock() {
    if (acquired_) {
      variable_->CheckResult(
          apr_global_mutex_unlock(variable_->mutex_), "unlock mutex");
    }
  }

  bool acquired() const { return acquired_; }

 private:
  const AprVariable* variable_;
  bool acquired_;

  DISALLOW_COPY_AND_ASSIGN(AprScopedGlobalLock);
};



AprVariable::AprVariable(const StringPiece& name)
    : mutex_(NULL), name_(name.as_string()), shm_(NULL), value_ptr_(NULL) {
}

int64 AprVariable::Get64() const {
  AprScopedGlobalLock lock(this);
  int64 value = lock.acquired() ? *value_ptr_ : -1;
  return value;
}

int AprVariable::Get() const {
  return Get64();
}

void AprVariable::Set(int newValue) {
  AprScopedGlobalLock lock(this);
  if (lock.acquired()) {
    *value_ptr_ = newValue;
  }
}

void AprVariable::Add(int delta) {
  AprScopedGlobalLock lock(this);
  if (lock.acquired()) {
    *value_ptr_ += delta;
  }
}

bool AprVariable::CheckResult(
    const apr_status_t result, const StringPiece& verb,
    const StringPiece& filename) const {
  if (result != APR_SUCCESS) {
   char buf[kStackBufferSize];
   apr_strerror(result, buf, sizeof(buf));
   LOG(ERROR) << "Variable " << name_ << " cannot " << verb << ": " << buf
              << " " << filename;
   return false;
  }
  return true;
}

bool AprVariable::InitMutex(const StringPiece& filename_prefix,
                            apr_pool_t* pool, bool parent) {
  // Create or attach to the mutex if we don't have one already.
  const char* filename = apr_pstrcat(
      pool, filename_prefix.as_string().c_str(), kStatisticsMutexPrefix,
      name_.c_str(), NULL);
  if (parent) {
    // We're being called from post_config.  Must create mutex.
    // Ensure the directory exists
    apr_dir_make(StrCat(filename_prefix, kStatisticsDir).c_str(),
                 APR_FPROT_OS_DEFAULT, pool);
    // TODO(abliss): do we need to destroy this mutex later?
    if (CheckResult(apr_global_mutex_create(
            &mutex_, filename, APR_LOCK_DEFAULT, pool),
                    "create mutex", filename)) {
      // On apache installations which (a) are unix-based, (b) use a
      // flock-based mutex, and (c) start the parent process as root but child
      // processes as a less-privileged user, we need this extra code to set
      // up the permissions of the lock.
#ifdef AP_NEED_SET_MUTEX_PERMS
      CheckResult(unixd_set_global_mutex_perms(mutex_), "chown mutex",
                  filename);
#endif
      return true;
    }
  } else {
    // We're being called from child_init.  Mutex must already exist.
    if (mutex_) {
      if (CheckResult(apr_global_mutex_child_init(&mutex_, filename, pool),
                      "attach mutex", filename)) {
        return true;
      } else {
        // Something went wrong; disable this variable by nulling its mutex.
        mutex_ = NULL;
      }
    } else {
      CheckResult(APR_ENOLOCK, "attach mutex", filename);
    }
  }
  return false;
}

bool AprVariable::InitShm(const StringPiece& filename_prefix,
                          apr_pool_t* pool, bool parent) {
  // On some platforms we inherit the existing segment...
  if (!shm_) {
    // ... but on others we must reattach to it.
    const char* filename = apr_pstrcat(
        pool, filename_prefix.as_string().c_str(), kStatisticsValuePrefix,
        name_.c_str(), NULL);
    if (parent) {
      // Sometimes the shm/file are leftover from a previous unclean exit.
      apr_shm_remove(filename, pool);
      apr_file_remove(filename, pool);
      int64 foo;
      // This shm is destroyed when apache is shutdown cleanly.
      CheckResult(apr_shm_create(&shm_, sizeof(foo), filename, pool),
                  "create shared memory", filename);
    } else {
      CheckResult(apr_shm_attach(&shm_, filename, pool),
                  "attach to shared memory", filename);
    }
  }
  if (shm_) {
    // value_ptr always needs to be reset, even if shm was inherited,
    // since its base address may have changed.
    value_ptr_ = reinterpret_cast<int64*>(apr_shm_baseaddr_get(shm_));
    return true;
  } else {
    // Something went wrong; disable this variable by nulling its mutex.
    mutex_ = NULL;
    return false;
  }
}

AprStatistics::AprStatistics(const StringPiece& filename_prefix)
    : frozen_(false), is_child_(false), filename_prefix_(filename_prefix) {
  apr_pool_create(&pool_, NULL);
}

AprStatistics::~AprStatistics() {
  if (!is_child_) {
    apr_pool_destroy(pool_);
  }
}

AprVariable* AprStatistics::NewVariable(const StringPiece& name, int index) {
  if (frozen_) {
    LOG(ERROR) << "Cannot add variable " << name
               << " after AprStatistics is frozen!";
    return NULL;
  } else {
    return new AprVariable(name);
  }
}

NullStatisticsHistogram* AprStatistics::NewHistogram() {
  return new NullStatisticsHistogram();
}

void AprStatistics::InitVariables(bool parent) {
  is_child_ |= !parent;
  if (frozen_) {
    return;
  }
  frozen_ = true;
  // Set up a global mutex and a shared-memory segment for each variable.
  for (int i = 0, n = variables_.size(); i < n; ++i) {
    AprVariable* var = variables_[i];
    if (!var->InitMutex(filename_prefix_, pool_, parent) ||
        !var->InitShm(filename_prefix_, pool_, parent)) {
      LOG(ERROR) << "Statistics initialization failed in pid " << getpid();
      return;
    }
  }
}

void AprStatistics::Dump(Writer* writer, MessageHandler* message_handler) {
  for (int i = 0, n = variables_.size(); i < n; ++i) {
    AprVariable* var = variables_[i];
    writer->Write(var->name(), message_handler);
    writer->Write(": ", message_handler);
    writer->Write(Integer64ToString(var->Get64()), message_handler);
    writer->Write("\n", message_handler);
  }
}

void AprStatistics::Clear() {
  for (int i = 0, n = variables_.size(); i < n; ++i) {
    AprVariable* var = variables_[i];
    var->Set(0);
  }
}

}  // namespace net_instaweb
