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

// Author: sligocki@google.com (Shawn Ligocki)

// Callbacks used for testing.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_MOCK_CALLBACK_H_
#define NET_INSTAWEB_HTTP_PUBLIC_MOCK_CALLBACK_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

// Callback that can be used for testing resource fetches which makes sure
// that Done() is called exactly once and with the expected success value.
// Can be used multiple times by calling Reset in between.
class ExpectStringAsyncFetch : public StringAsyncFetch {
 public:
  ExpectStringAsyncFetch(bool expect_success,
                         const RequestContextPtr& request_context)
      : StringAsyncFetch(request_context), expect_success_(expect_success) {}
  virtual ~ExpectStringAsyncFetch() {
    EXPECT_TRUE(done());
  }

  virtual void HandleDone(bool success) {
    EXPECT_FALSE(done()) << "Already Done; perhaps you reused without Reset()";
    StringAsyncFetch::HandleDone(success);
    EXPECT_EQ(expect_success_, success);
  }

  void set_expect_success(bool x) { expect_success_ = x; }

 private:
  bool expect_success_;

  DISALLOW_COPY_AND_ASSIGN(ExpectStringAsyncFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_MOCK_CALLBACK_H_
