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

#include "pagespeed/kernel/cache/cache_spammer.h"

#include <memory>
#include <vector>

#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_test_base.h"

namespace net_instaweb {

CacheSpammer::CacheSpammer(ThreadSystem* runtime,
                           ThreadSystem::ThreadFlags flags,
                           CacheInterface* cache,
                           bool expecting_evictions,
                           bool do_deletes,
                           const char* value_pattern,
                           int index,
                           int num_iters,
                           int num_inserts)
    : Thread(runtime, "cache_spammer", flags),
      cache_(cache),
      expecting_evictions_(expecting_evictions),
      do_deletes_(do_deletes),
      value_pattern_(value_pattern),
      index_(index),
      num_iters_(num_iters),
      num_inserts_(num_inserts) {
}

CacheSpammer::~CacheSpammer() {
}

void CacheSpammer::RunTests(int num_threads,
                            int num_iters,
                            int num_inserts,
                            bool expecting_evictions, bool do_deletes,
                            const char* value_pattern,
                            CacheInterface* cache,
                            ThreadSystem* thread_runtime) {
  std::vector<CacheSpammer*> spammers(num_threads);

  // First, create all the threads.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i] = new CacheSpammer(
        thread_runtime, ThreadSystem::kJoinable,
        cache,  // lru_cache_.get() will make this fail.
        expecting_evictions, do_deletes, value_pattern, i, num_iters,
        num_inserts);
  }

  // Then, start them.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i]->Start();
  }

  // Finally, wait for them to complete by joining them.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i]->Join();
    delete spammers[i];
  }
}

void CacheSpammer::Run() {
  const char name_pattern[] = "name%d";
  std::vector<SharedString> inserts(num_inserts_);
  for (int j = 0; j < num_inserts_; ++j) {
    inserts[j].Assign(StringPrintf(value_pattern_, j));
  }

  int iter_limit = RunningOnValgrind() ? num_iters_ / 100 : num_iters_;
  for (int i = 0; i < iter_limit; ++i) {
    for (int j = 0; j < num_inserts_; ++j) {
      cache_->Put(StringPrintf(name_pattern, j), &inserts[j]);
    }
    for (int j = 0; j < num_inserts_; ++j) {
      // Ignore the result.  Thread interactions will make it hard to
      // predict if the Get will succeed or not.
      GoogleString key = StringPrintf(name_pattern, j);
      CacheTestBase::Callback callback;
      cache_->Get(key, &callback);
      bool found = (callback.called() &&
                    (callback.state() == CacheInterface::kAvailable));

      // We cannot assume that a Get succeeds if there are evictions
      // or deletions going on.  But we are still verifying that the code
      // will not crash, and that after the threads have all quiesced,
      // that the cache is still sane.
      EXPECT_TRUE(found || expecting_evictions_ || do_deletes_)
          << "Failed on key " << key << " i=" << i << " j=" << j
          << " thread=" << index_;
    }
    if (do_deletes_) {
      for (int j = 0; j < num_inserts_; ++j) {
        cache_->Delete(StringPrintf(name_pattern, j));
      }
    }
  }
}

}  // namespace net_instaweb
