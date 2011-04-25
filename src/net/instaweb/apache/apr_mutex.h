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

#ifndef NET_INSTAWEB_APACHE_APR_MUTEX_H_
#define NET_INSTAWEB_APACHE_APR_MUTEX_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/abstract_mutex.h"

// Forward declaration.
struct apr_thread_mutex_t;
struct apr_pool_t;

namespace net_instaweb {

class AprMutex : public net_instaweb::AbstractMutex {
 public:
  explicit AprMutex(apr_pool_t* pool);
  virtual ~AprMutex();
  virtual void Lock();
  virtual void Unlock();
 private:
  friend class AprCondvar;

  apr_thread_mutex_t* thread_mutex_;

  DISALLOW_COPY_AND_ASSIGN(AprMutex);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_MUTEX_H_
