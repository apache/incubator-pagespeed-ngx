/*
 * Copyright 2012 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

// Mock PropertyPage for use in unit tests.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_PROPERTY_PAGE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_PROPERTY_PAGE_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MockPropertyPage : public PropertyPage {
 public:
  MockPropertyPage(ThreadSystem* thread_system,
                   PropertyCache* property_cache,
                   const StringPiece& key)
      : PropertyPage(
          kPropertyCachePage, key,
          RequestContext::NewTestRequestContext(thread_system),
          thread_system->NewMutex(), property_cache),
        called_(false),
        valid_(false),
        time_ms_(-1) {}
  virtual ~MockPropertyPage();
  virtual bool IsCacheValid(int64 write_timestamp_ms) const {
    return time_ms_ == -1 || write_timestamp_ms > time_ms_;
  }
  virtual void Done(bool valid) {
    called_ = true;
    valid_ = valid;
  }

  bool called() const { return called_; }
  bool valid() const { return valid_; }
  void set_time_ms(int64 time_ms) {
    time_ms_ = time_ms;
  }

 private:
  bool called_;
  bool valid_;
  int64 time_ms_;

  DISALLOW_COPY_AND_ASSIGN(MockPropertyPage);
};
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_PROPERTY_PAGE_H_
