/*
 * Copyright 2013 Google Inc.
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

// Author: jkarlin@google.com (Josh Karlin)

#include "net/instaweb/rewriter/public/test_distributed_fetcher.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// AsyncFetch implementation that allows for an optional failure test, failure
// to write anything after the response headers have been written, set with the
// set_fail_after_headers function. In the non-failure case, all calls pass
// through to the given base fetch.
class TestDistributedFetcher::TestDistributedFetch : public SharedAsyncFetch {
 public:
  explicit TestDistributedFetch(AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch),
        fail_after_headers_(false),
        failed_(false) {}

  virtual ~TestDistributedFetch() {}

  virtual void HandleHeadersComplete() {
    SharedAsyncFetch::HandleHeadersComplete();
    if (fail_after_headers_) {
      failed_ = true;
    }
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    if (failed_) {
      return true;
    } else {
      return SharedAsyncFetch::HandleWrite(content, handler);
    }
  }

  virtual void HandleDone(bool success) {
    // Fail if failed_ otherwise use success.
    SharedAsyncFetch::HandleDone(success && !failed_);
    delete this;
  }

  // If true, stops writing after headers have been written.
  void set_fail_after_headers(bool x) { fail_after_headers_ = x; }

 private:
  bool fail_after_headers_;  // If true, write nothing after response headers.
  bool failed_;  // State used to stop writing after response header.
};


TestDistributedFetcher::TestDistributedFetcher(
    RewriteTestBase* rewrite_test_base)
    : rewrite_test_base_(rewrite_test_base),
      fail_after_headers_(false),
      blocking_fetch_(true) {
}

TestDistributedFetcher::~TestDistributedFetcher() {}

void TestDistributedFetcher::Fetch(const GoogleString& url,
                                   MessageHandler* message_handler,
                                   AsyncFetch* fetch) {
  // Call FetchResource on the test's other rewrite driver.
  DCHECK(rewrite_test_base_ != NULL);
  RewriteDriver* other_driver = rewrite_test_base_->other_rewrite_driver();
  TestRewriteDriverFactory* other_factory = rewrite_test_base_->other_factory();
  TestDistributedFetch* test_fetch = new TestDistributedFetch(fetch);
  test_fetch->set_fail_after_headers(fail_after_headers_);
  other_driver->SetRequestHeaders(*fetch->request_headers());

  other_driver->FetchResource(url, test_fetch);
  if (blocking_fetch_) {
    // Simulate instantaneous response. Otherwise we don't know when an object
    // is committed to the shared cache in testing, making it impossible to
    // properly count cache hits/misses.
    other_driver->WaitForShutDown();
    other_factory->mock_scheduler()->AwaitQuiescence();
  }
}

}  // namespace net_instaweb
