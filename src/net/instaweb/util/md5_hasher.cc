// Copyright 2010 Google Inc.
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
//
// Authors: sligocki@google.com (Shawn Ligocki),
//          lsong@google.com (Libo Song)

#include "net/instaweb/util/public/md5_hasher.h"

#include "net/instaweb/util/public/base64_util.h"

#include "base/md5.h"

namespace net_instaweb {

namespace {

const int kMD5NumBytes = sizeof(MD5Digest);

}  // namespace

// Hash size is size after Base64 encoding, which expands by 4/3. We round down,
// this should not matter unless someone really wants that extra few bits.
const int MD5Hasher::kMaxHashSize = kMD5NumBytes * 4 / 3;

MD5Hasher::~MD5Hasher() {
}

std::string MD5Hasher::Hash(const StringPiece& content) const {
  // Note:  It may seem more efficient to initialize the MD5Context
  // in a constructor so it can be re-used.  But a quick inspection
  // of src/third_party/chromium/src/base/md5.cc indicates that
  // the cost of MD5Init is very tiny compared to the cost of
  // MD5Update, so it's better to stay thread-safe.
  MD5Digest digest;
  MD5Sum(content.data(), content.size(), &digest);
  // Note: digest.a is an unsigned char[16] so it's not null-terminated.
  StringPiece raw_hash(reinterpret_cast<char*>(digest.a), sizeof(digest.a));
  std::string out;
  Web64Encode(raw_hash, &out);
  out.resize(hash_size_);
  return out;
}

}  // namespace net_instaweb
