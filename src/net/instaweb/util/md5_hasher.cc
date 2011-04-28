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

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "base/md5.h"

namespace net_instaweb {

namespace {

const int kMD5NumBytes = sizeof(MD5Digest);

}  // namespace

MD5Hasher::~MD5Hasher() {
}

GoogleString MD5Hasher::RawHash(const StringPiece& content) const {
  // Note:  It may seem more efficient to initialize the MD5Context
  // in a constructor so it can be re-used.  But a quick inspection
  // of src/third_party/chromium/src/base/md5.cc indicates that
  // the cost of MD5Init is very tiny compared to the cost of
  // MD5Update, so it's better to stay thread-safe.
  MD5Digest digest;
  MD5Sum(content.data(), content.size(), &digest);
  // Note: digest.a is an unsigned char[16] so it's not null-terminated.
  GoogleString raw_hash(reinterpret_cast<char*>(digest.a), sizeof(digest.a));
  return raw_hash;
}

int MD5Hasher::RawHashSizeInBytes() const {
  return kMD5NumBytes;
}

}  // namespace net_instaweb
