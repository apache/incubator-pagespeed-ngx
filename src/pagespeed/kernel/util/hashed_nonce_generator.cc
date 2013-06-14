// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "pagespeed/kernel/util/hashed_nonce_generator.h"

#include <unistd.h>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

HashedNonceGenerator::HashedNonceGenerator(
    const Hasher* hasher, StringPiece key, AbstractMutex* mutex)
    : NonceGenerator(mutex),
      hasher_(hasher),
      key_size_(key.size() - sizeof(counter_)) {
  DCHECK_LE(2 * hasher->RawHashSizeInBytes(), key.size());
  // Allocate enough space in key_ to hold future data for hashing.  Note in
  // particular that we use the last sizeof(counter_) bits of the passed in key
  // to initialize counter_, and thus exclude it from key_size_.  But we will
  // ultimately need to buffer the initial key_size_ bytes of the passed-in key,
  // the counter_, and the pid.
  char *key_data = new char[key_size_ + sizeof(counter_) + sizeof(pid_t)];
  memcpy(key_data, key.data(), key_size_);
  memcpy(&counter_, key.data() + key_size_, sizeof(counter_));
  key_.reset(key_data);
  counter_ = hasher_->HashToUint64(key);
}

HashedNonceGenerator::~HashedNonceGenerator() { }

uint64 HashedNonceGenerator::NewNonceImpl() {
  ++counter_;
  // We look up the pid here and include it because we may have forked since
  // this HashedNonceGenerator was created, and we want to make sure the forked
  // values yield distinct nonce streams.
  pid_t pid = getpid();
  // We append data to key_, then hash the whole buffer.
  memcpy(&key_[key_size_], &counter_, sizeof(counter_));
  memcpy(&key_[key_size_ + sizeof(counter_)], &pid, sizeof(pid));
  return hasher_->HashToUint64(
      StringPiece(key_.get(), key_size_ + sizeof(counter_) + sizeof(pid)));
}

}  // namespace net_instaweb
