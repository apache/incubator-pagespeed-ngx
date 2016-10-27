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

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

CacheSpammer::CacheSpammer(ThreadSystem* runtime,
                           ThreadSystem::ThreadFlags flags,
                           CacheInterface* cache,
                           bool expecting_evictions,
                           bool do_deletes,
                           const char* value_prefix,
                           int index,
                           int num_iters,
                           int num_inserts)
    : Thread(runtime, "cache_spammer", flags),
      cache_(cache),
      expecting_evictions_(expecting_evictions),
      do_deletes_(do_deletes),
      value_prefix_(value_prefix),
      index_(index),
      num_iters_(num_iters),
      num_inserts_(num_inserts),
      mutex_(runtime->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      pending_gets_(0) {
}

CacheSpammer::~CacheSpammer() {
}

namespace {

class SpammerCallback : public CacheInterface::Callback {
 public:
  SpammerCallback(CacheSpammer* spammer, StringPiece key, StringPiece expected)
      : spammer_(spammer),
        validate_candidate_called_(false),
        key_(key.data(), key.size()),
        expected_(expected.data(), expected.size()) {
  }

  virtual ~SpammerCallback() {
  }

  virtual void Done(CacheInterface::KeyState state) {
    DCHECK(validate_candidate_called_);
    bool found = (state == CacheInterface::kAvailable);
    if (found) {
      EXPECT_STREQ(expected_, value().Value());
    }
    spammer_->GetDone(found, key_);
    delete this;
  }

  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    validate_candidate_called_ = true;
    return true;
  }

 private:
  CacheSpammer* spammer_;
  bool validate_candidate_called_;
  GoogleString key_;
  GoogleString expected_;

  DISALLOW_COPY_AND_ASSIGN(SpammerCallback);
};

}  // namespace

void CacheSpammer::RunTests(int num_threads,
                            int num_iters,
                            int num_inserts,
                            bool expecting_evictions, bool do_deletes,
                            const char* value_prefix,
                            CacheInterface* cache,
                            ThreadSystem* thread_runtime) {
  std::vector<CacheSpammer*> spammers(num_threads);

  // First, create all the threads.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i] = new CacheSpammer(
        thread_runtime, ThreadSystem::kJoinable,
        cache,  // lru_cache_.get() will make this fail.
        expecting_evictions, do_deletes, value_prefix, i, num_iters,
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
    inserts[j].Assign(StringPrintf("%s%d", value_prefix_, j));
  }

  int iter_limit = RunningOnValgrind() ? num_iters_ / 100 : num_iters_;
  for (int i = 0; i < iter_limit; ++i) {
    for (int j = 0; j < num_inserts_; ++j) {
      cache_->Put(StringPrintf(name_pattern, j), inserts[j]);
    }
    {
      ScopedMutex lock(mutex_.get());
      pending_gets_ = num_inserts_;
    }
    for (int j = 0; j < num_inserts_; ++j) {
      // Ignore the result.  Thread interactions will make it hard to
      // predict if the Get will succeed or not.
      GoogleString key = StringPrintf(name_pattern, j);
      cache_->Get(key, new SpammerCallback(this, key, inserts[j].Value()));
    }
    {
      ScopedMutex lock(mutex_.get());
      while (pending_gets_ != 0) {
        condvar_->Wait();
      }
    }
    if (do_deletes_) {
      for (int j = 0; j < num_inserts_; ++j) {
        cache_->Delete(StringPrintf(name_pattern, j));
      }
    }
  }
}

void CacheSpammer::GetDone(bool found, StringPiece key) {
  ScopedMutex lock(mutex_.get());
  --pending_gets_;
  // We cannot assume that a Get succeeds if there are evictions
  // or deletions going on.  But we are still verifying that the code
  // will not crash, and that after the threads have all quiesced,
  // that the cache is still sane.
  EXPECT_TRUE(found || expecting_evictions_ || do_deletes_)
      << "Failed on key " << key;
  if (pending_gets_ == 0) {
    condvar_->Signal();
  }
}

}  // namespace net_instaweb
