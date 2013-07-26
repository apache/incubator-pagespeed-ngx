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

// Author: jmarantz@google.com (Joshua Marantz)
//
// Tests the speed of LRUCache, using different insert-sizes & key sizes.
//
//
// Benchmark              Time(ns)    CPU(ns) Iterations
// -----------------------------------------------------
// LRUPuts               77892025   77700000        100
// LRUReplaceSameValue  140400882  140000000        100
// LRUReplaceNewValue   139482372  139100000        100
// LRUGets               43501155   43400000        100
// LRUFailedGets         16068878   16000000        100
// LRUEvictions         143558421  143200000        100

#include "pagespeed/kernel/cache/lru_cache.h"

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/simple_random.h"

namespace {

const int kNumKeys = 100000;
const int kKeySize = 50;
const int kPayloadSize = 100;

class EmptyCallback : public net_instaweb::CacheInterface::Callback {
 public:
  EmptyCallback() {}
  virtual ~EmptyCallback() {}
  virtual void Done(net_instaweb::CacheInterface::KeyState state) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyCallback);
};

class TestPayload {
 public:
  TestPayload(int key_size, int payload_size, int num_keys, bool do_puts)
      : random_(new net_instaweb::NullMutex),
        cache_size_((key_size + payload_size) * num_keys),
        key_size_(key_size),
        num_keys_(num_keys),
        start_index_(0),
        lru_cache_(cache_size_),
        keys_(num_keys),
        values_(num_keys) {
    StopBenchmarkTiming();
    RegenerateKeys();
    GoogleString key_prefix = random_.GenerateHighEntropyString(key_size);
    GoogleString value_prefix = random_.GenerateHighEntropyString(payload_size);
    for (int k = 0; k < num_keys_; ++k) {
      GoogleString value = value_prefix;
      OverwriteIndexAtEndOfString(&value, k);
      values_[k].SwapWithString(&value);
      keys_[k] = key_prefix;
    }
    RegenerateKeys();
    if (do_puts) {
      DoPuts(0);
    }
    StartBenchmarkTiming();
  }

  void OverwriteIndexAtEndOfString(GoogleString* buffer, int index) {
    GoogleString index_string =
        net_instaweb::StrCat("_", net_instaweb::IntegerToString(index));
    DCHECK_LT(index_string.size(), buffer->size());
    char* ptr = &(*buffer)[buffer->size() - index_string.size()];
    memcpy(ptr, index_string.data(), index_string.size());
  }

  void RegenerateKeys() {
    for (int k = 0; k < num_keys_; ++k) {
      OverwriteIndexAtEndOfString(&keys_[k], k + start_index_);
    }
    start_index_ += num_keys_;
  }

  void DoPuts(int rotate_by) {
    for (int k = 0; k < num_keys_; ++k) {
      lru_cache_.Put(keys_[(k + rotate_by) % num_keys_], &values_[k]);
    }
  }

  void DoGets() {
    for (int k = 0; k < num_keys_; ++k) {
      lru_cache_.Get(keys_[k], &empty_callback_);
    }
  }

  net_instaweb::LRUCache* lru_cache() { return &lru_cache_; }

 private:
  net_instaweb::SimpleRandom random_;
  int cache_size_;
  int key_size_;
  int num_keys_;
  int start_index_;
  net_instaweb::LRUCache lru_cache_;
  net_instaweb::StringVector keys_;
  std::vector<net_instaweb::SharedString> values_;
  EmptyCallback empty_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPayload);
};

static void LRUPuts(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, false);
  for (int i = 0; i < iters; ++i) {
    payload.lru_cache()->Clear();
    payload.DoPuts(0);
  }
  CHECK_EQ(0, static_cast<int>(payload.lru_cache()->num_evictions()));
}

static void LRUReplaceSameValue(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, true);
  for (int i = 0; i < iters; ++i) {
    payload.DoPuts(0);
  }
  CHECK_LT(0, static_cast<int>(payload.lru_cache()->num_identical_reinserts()));
  CHECK_EQ(0, static_cast<int>(payload.lru_cache()->num_evictions()));
}

static void LRUReplaceNewValue(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, true);
  for (int i = 0; i < iters; ++i) {
    payload.DoPuts(i + 1);
  }
  CHECK_EQ(0, static_cast<int>(payload.lru_cache()->num_identical_reinserts()));
  CHECK_EQ(0, static_cast<int>(payload.lru_cache()->num_evictions()));
}

static void LRUGets(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, true);
  for (int i = 0; i < iters; ++i) {
    payload.DoGets();
  }
  CHECK_EQ(kNumKeys * iters, static_cast<int>(payload.lru_cache()->num_hits()));
}

static void LRUFailedGets(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, true);
  payload.RegenerateKeys();
  for (int i = 0; i < iters; ++i) {
    payload.DoGets();
  }
  CHECK_EQ(0, static_cast<int>(payload.lru_cache()->num_hits()));
}

static void LRUEvictions(int iters) {
  TestPayload payload(kKeySize, kPayloadSize, kNumKeys, true);
  for (int i = 0; i < iters; ++i) {
    payload.RegenerateKeys();
    payload.DoPuts(0);
  }

  CHECK_LT(0, static_cast<int>(payload.lru_cache()->num_evictions()));
}

}  // namespace

BENCHMARK(LRUPuts);
BENCHMARK(LRUReplaceSameValue);
BENCHMARK(LRUReplaceNewValue);
BENCHMARK(LRUGets);
BENCHMARK(LRUFailedGets);
BENCHMARK(LRUEvictions);
