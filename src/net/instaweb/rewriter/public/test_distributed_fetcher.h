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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_TEST_DISTRIBUTED_FETCHER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_TEST_DISTRIBUTED_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;

class RewriteTestBase;

// The Fetch implementation that talks directly to the rewrite task via
// RewriteDriver::FetchResource as opposed to talking to it over the network.
// The rewrite task in the test environment is
// RewriteTestBase::other_rewrite_driver_.
class TestDistributedFetcher : public UrlAsyncFetcher {
 public:
  explicit TestDistributedFetcher(RewriteTestBase* test_base);
  virtual ~TestDistributedFetcher();
  virtual void Fetch(const GoogleString& url, MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // If true, stops writing to the fetch after the headers and HandleDone's
  // success parameter will be false.
  void set_fail_after_headers(bool x) { fail_after_headers_ = x; }

  // Should the fetch block on the distributed rewrite? We usually want this to
  // be true because that way we can predict the behavior of the shared cache in
  // our tests but some tests require it to be false.
  void set_blocking_fetch(bool x) { blocking_fetch_ = x; }

 private:
  class TestDistributedFetch;
  RewriteTestBase* rewrite_test_base_;
  bool fail_after_headers_;
  bool blocking_fetch_;
  DISALLOW_COPY_AND_ASSIGN(TestDistributedFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_TEST_DISTRIBUTED_FETCHER_H_
