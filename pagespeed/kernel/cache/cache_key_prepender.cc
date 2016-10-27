/*
 * Copyright 2016 Google Inc.
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

// Author: yeputons@google.com (Egor Suvorov)

#include "pagespeed/kernel/cache/cache_key_prepender.h"

#include "base/logging.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/delegating_cache_callback.h"

namespace net_instaweb {

CacheKeyPrepender::CacheKeyPrepender(StringPiece prefix, CacheInterface* cache)
    : cache_(cache), prefix_(prefix) {}

GoogleString CacheKeyPrepender::FormatName(StringPiece prefix,
                                           StringPiece cache) {
  return StrCat("KeyPrepender(prefix=", prefix, ",cache=", cache, ")");
}

class CacheKeyPrepender::KeyPrependerCallback : public DelegatingCacheCallback {
 public:
  KeyPrependerCallback(CacheInterface::Callback* callback,
                       const SharedString& prefix)
      : DelegatingCacheCallback(callback), prefix_(prefix) {}
  ~KeyPrependerCallback() override {}

  bool ValidateCandidate(const GoogleString& key,
                         CacheInterface::KeyState state) override {
    if (!strings::StartsWith(key, prefix_.Value())) {
      LOG(DFATAL) << "KeyPrependerCallback has received a key without expected "
                 << "prefix, treating as cache miss";
      return false;
    }
    return DelegatingCacheCallback::ValidateCandidate(
        key.substr(prefix_.size()), state);
  }

 private:
  SharedString prefix_;

  DISALLOW_COPY_AND_ASSIGN(KeyPrependerCallback);
};

void CacheKeyPrepender::Get(const GoogleString& key, Callback* callback) {
  // KeyPrependerCallback deletes itself after it's fired.
  cache_->Get(AddPrefix(key), new KeyPrependerCallback(callback, prefix_));
}

void CacheKeyPrepender::MultiGet(MultiGetRequest* request) {
  for (KeyCallback& key_callback : *request) {
    key_callback.key = AddPrefix(key_callback.key);
    // KeyPrependerCallback deletes itself after it's fired.
    key_callback.callback =
        new KeyPrependerCallback(key_callback.callback, prefix_);
  }
  cache_->MultiGet(request);
}

void CacheKeyPrepender::Put(const GoogleString& key,
                            const SharedString& value) {
  cache_->Put(AddPrefix(key), value);
}

void CacheKeyPrepender::Delete(const GoogleString& key) {
  cache_->Delete(AddPrefix(key));
}

GoogleString CacheKeyPrepender::AddPrefix(const GoogleString& key) {
  return StrCat(prefix_.Value(), key);
}

}  // namespace net_instaweb
