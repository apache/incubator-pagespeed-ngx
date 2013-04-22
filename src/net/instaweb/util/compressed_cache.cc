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

#include "net/instaweb/util/public/compressed_cache.h"

#include "base/logging.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

// A few bytes to put at the end of the physical payload we can track
// corruption.  Note that CompressedCacheTest.CrapAtEnd fails without this.
const char kTrailer[] = "[[]]";

// TODO(jmarantz): Evaluate the impact of histogramming the size reduction of
// each entry.  The compressed_cache_speed_test.cc side-steps this because
// SimpleStats doesn't implement histograms.  See split_statistics_test.cc:93
// for mechanism to speed-test histograms.
#define INCLUDE_HISTOGRAMS 0

#if INCLUDE_HISTOGRAMS
const char kCompressedCacheSavings[] = "compressed_cache_savings";
#endif
const char kCompressedCacheOriginalSize[] = "compressed_cache_original_size";
const char kCompressedCacheCompressedSize[] =
    "compressed_cache_compressed_size";
const char kCompressedCacheCorruptPayloads[] =
    "compressed_cache_corrupt_payloads";

class CompressedCallback : public CacheInterface::Callback {
 public:
  CompressedCallback(CacheInterface::Callback* callback,
                     Variable* corrupt_payloads)
      : callback_(callback),
        corrupt_payloads_(corrupt_payloads),
        validate_candidate_called_(false) {
  }

  virtual ~CompressedCallback() {}

  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    validate_candidate_called_ = true;
    bool ret = false;
    GoogleString uncompressed;
    StringWriter writer(&uncompressed);
    if (state == CacheInterface::kAvailable) {
      StringPiece compressed = value()->Value();
      if (compressed.ends_with(StringPiece(kTrailer,
                                           STATIC_STRLEN(kTrailer))) &&
          GzipInflater::Inflate(compressed.substr(
              0, compressed.size() - STATIC_STRLEN(kTrailer)), &writer)) {
        callback_->value()->SwapWithString(&uncompressed);
        ret = true;
      } else {
        state = CacheInterface::kNotFound;
        corrupt_payloads_->Add(1);
      }
    }
    ret &= callback_->DelegatedValidateCandidate(key, state);
    return ret;
  }

  virtual void Done(CacheInterface::KeyState state) {
    DCHECK(validate_candidate_called_);
    callback_->DelegatedDone(state);
    delete this;
  }

  Callback* callback_;
  Variable* corrupt_payloads_;
  bool validate_candidate_called_;
};

}  // namespace

CompressedCache::CompressedCache(CacheInterface* cache, Statistics* stats)
    : cache_(cache) {
#if INCLUDE_HISTOGRAMS
  compressed_cache_savings_ = stats->GetHistogram(kCompressedCacheSavings);
#endif
  corrupt_payloads_ = stats->GetVariable(kCompressedCacheCorruptPayloads);
  original_size_ = stats->GetVariable(kCompressedCacheOriginalSize);
  compressed_size_ = stats->GetVariable(kCompressedCacheCompressedSize);
}

CompressedCache::~CompressedCache() {
}

GoogleString CompressedCache::FormatName(StringPiece name) {
  return StrCat("Compressed(", name, ")");
}

void CompressedCache::InitStats(Statistics* statistics) {
#if INCLUDE_HISTOGRAMS
  statistics->AddHistogram(kCompressedCacheSavings);
#endif
  statistics->AddVariable(kCompressedCacheCorruptPayloads);
  statistics->AddVariable(kCompressedCacheOriginalSize);
  statistics->AddVariable(kCompressedCacheCompressedSize);
}

void CompressedCache::Get(const GoogleString& key, Callback* callback) {
  CompressedCallback* cb = new CompressedCallback(callback, corrupt_payloads_);
  cache_->Get(key, cb);
}

void CompressedCache::Put(const GoogleString& key, SharedString* value) {
  int64 old_size = value->size();
  GoogleString buf;
  buf.reserve(old_size + STATIC_STRLEN(kTrailer));
  StringWriter writer(&buf);
  original_size_->Add(old_size);
  if (GzipInflater::Deflate(value->Value(), &writer)) {
    buf.append(kTrailer, STATIC_STRLEN(kTrailer));
#if INCLUDE_HISTOGRAMS
    compressed_cache_savings_->Add(
        old_size - static_cast<int64>(buf.size()));
#endif
    compressed_size_->Add(buf.size());
    cache_->PutSwappingString(key, &buf);
  }
}

void CompressedCache::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

bool CompressedCache::IsHealthy() const {
  return cache_->IsHealthy();
}

void CompressedCache::ShutDown() {
  return cache_->ShutDown();
}

int64 CompressedCache::CorruptPayloads() const {
  return corrupt_payloads_->Get();
}

int64 CompressedCache::OriginalSize() const {
  return original_size_->Get();
}

int64 CompressedCache::CompressedSize() const {
  return compressed_size_->Get();
}

}  // namespace net_instaweb
