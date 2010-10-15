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
#include "apr.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/stack_buffer.h"
#include "base/logging.h"
extern "C" {
#include "httpd.h"
#include "http_config.h"
#ifdef AP_NEED_SET_MUTEX_PERMS
#include "unixd.h"
#endif
}


namespace {

const char* kStatisticsDir = "mod_instaweb";
const char* kStatisticsMutexPrefix = "mod_instaweb/stats_mutex.";
const char* kStatisticsValuePrefix = "mod_instaweb/stats_value.";

}  // namespace

namespace net_instaweb {

AprVariable::AprVariable(const StringPiece& name)
    : mutex_(NULL), name_(name.as_string()), shm_(NULL), value_ptr_(NULL) {
}

int64 AprVariable::Get64() const {
  int64 value = -1;
  if (mutex_ == NULL) {
    // This variable was not properly initialized.
    return value;
  }
  if (!CheckResult(apr_global_mutex_lock(mutex_), "lock mutex")) {
    return value;
  }
  value = *value_ptr_;
  CheckResult(apr_global_mutex_unlock(mutex_), "unlock mutex");
  return value;
}

int AprVariable::Get() const {
  return Get64();
}

void AprVariable::Set(int newValue) {
  if (mutex_ == NULL) {
    // This variable was not properly initialized.
    return;
  }
  if (!CheckResult(apr_global_mutex_lock(mutex_), "lock mutex")) {
    return;
  }
  *value_ptr_ = newValue;
  CheckResult(apr_global_mutex_unlock(mutex_), "unlock mutex");
}

void AprVariable::Add(int delta) {
  if (mutex_ == NULL) {
    // This variable was not properly initialized.
    return;
  }
  if (!CheckResult(apr_global_mutex_lock(mutex_), "lock mutex")) {
    return;
  }
  *value_ptr_ += delta;
  CheckResult(apr_global_mutex_unlock(mutex_), "unlock mutex");
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

bool AprVariable::InitMutex(apr_pool_t* pool, bool parent) {
  // Create or attach to the mutex if we don't have one already.
  const char* filename = ap_server_root_relative(
      pool, StrCat(kStatisticsMutexPrefix, name_).c_str());
  if (parent) {
    // We're being called from post_config.  Must create mutex.
    // Ensure the directory exists
    apr_dir_make(ap_server_root_relative(
        pool, kStatisticsDir), APR_FPROT_OS_DEFAULT, pool);
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

bool AprVariable::InitShm(apr_pool_t* pool, bool parent) {
  // On some platforms we inherit the existing segment...
  if (!shm_) {
    // ... but on others we must reattach to it.
    const char* filename = ap_server_root_relative(
        pool, StrCat(kStatisticsValuePrefix, name_).c_str());
    if (parent) {
      int64 foo;
      // TODO(abliss): do we need to destroy this shm later?
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

AprStatistics::AprStatistics() : frozen_(false) {
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

// TODO(abliss): What is the lifetime of this pool? Are we leaking memory?
void AprStatistics::InitVariables(apr_pool_t* pool, bool parent) {
  if (frozen_) {
    return;
  }
  // Set up a global mutex and a shared-memory segment for each variable.
  for (int i = 0, n = variables_.size(); i < n; ++i) {
    AprVariable* var = variables_[i];
    if (!var->InitMutex(pool, parent) ||
        !var->InitShm(pool, parent)) {
      LOG(ERROR) << "Variable " << var->name() << " will not increment in PID "
                 << getpid();
    }
  }
  frozen_ = true;
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
