/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/http/public/counting_url_async_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

class CountingUrlAsyncFetcher::CountingFetch : public SharedAsyncFetch {
 public:
  CountingFetch(CountingUrlAsyncFetcher* counter, AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch), counter_(counter) {
    ScopedMutex lock(counter_->mutex_.get());
    ++counter_->fetch_start_count_;
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    {
      ScopedMutex lock(counter_->mutex_.get());
      counter_->byte_count_ += content.size();
    }
    return SharedAsyncFetch::HandleWrite(content, handler);
  }

  virtual void HandleDone(bool success) {
    {
      ScopedMutex lock(counter_->mutex_.get());
      ++counter_->fetch_count_;
      if (!success) {
        ++counter_->failure_count_;
      }
    }
    SharedAsyncFetch::HandleDone(success);
    delete this;
  }

 private:
  CountingUrlAsyncFetcher* counter_;

  DISALLOW_COPY_AND_ASSIGN(CountingFetch);
};

CountingUrlAsyncFetcher::~CountingUrlAsyncFetcher() {
}

void CountingUrlAsyncFetcher::Fetch(const GoogleString& url,
                                    MessageHandler* message_handler,
                                    AsyncFetch* base_fetch) {
  {
    ScopedMutex lock(mutex_.get());
    most_recent_fetched_url_ = url;
  }
  CountingFetch* counting_fetch = new CountingFetch(this, base_fetch);
  fetcher_->Fetch(url, message_handler, counting_fetch);
}

void CountingUrlAsyncFetcher::Clear() {
  ScopedMutex lock(mutex_.get());
  fetch_count_ = 0;
  fetch_start_count_ = 0;
  byte_count_ = 0;
  failure_count_ = 0;
  most_recent_fetched_url_ = "";
}

}  // namespace net_instaweb
