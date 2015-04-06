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

#ifndef PAGESPEED_OPT_HTTP_MOCK_PROPERTY_PAGE_H_
#define PAGESPEED_OPT_HTTP_MOCK_PROPERTY_PAGE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

class MockPropertyPage : public PropertyPage {
 public:
  MockPropertyPage(ThreadSystem* thread_system,
                   PropertyCache* property_cache,
                   const StringPiece& url,
                   const StringPiece& options_signature_hash,
                   const StringPiece& cache_key_suffix)
      : PropertyPage(
          kPropertyCachePage,
          url,
          options_signature_hash,
          cache_key_suffix,
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

#endif  // PAGESPEED_OPT_HTTP_MOCK_PROPERTY_PAGE_H_
