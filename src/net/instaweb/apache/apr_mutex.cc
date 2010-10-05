// Copyright 2010 Google Inc.
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

#include "net/instaweb/apache/apr_mutex.h"

#include "apr_pools.h"
#include "apr_thread_mutex.h"

namespace html_rewriter {

AprMutex::AprMutex(apr_pool_t* pool) {
  apr_thread_mutex_create(&thread_mutex_, APR_THREAD_MUTEX_DEFAULT, pool);
}

AprMutex::~AprMutex() {
  apr_thread_mutex_destroy(thread_mutex_);
}

void AprMutex::Lock() {
    apr_thread_mutex_lock(thread_mutex_);
}

void AprMutex::Unlock() {
    apr_thread_mutex_unlock(thread_mutex_);
}

}  // namespace html_rewriter
