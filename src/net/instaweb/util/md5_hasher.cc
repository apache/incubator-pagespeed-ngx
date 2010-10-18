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

#include "base/md5.h"

namespace net_instaweb {

MD5Hasher::~MD5Hasher() {
}

std::string MD5Hasher::Hash(const StringPiece& content) const {
  // Note:  It may seem more efficient to initialize the MD5Context
  // in a constructor so it can be re-used.  But a quick inspection
  // of src/third_party/chromium/src/base/md5.cc indicates that
  // the cost of MD5Init is very tiny compared to the cost of
  // MD5Update, so it's better to stay thread-safe.
  MD5Context ctx;
  MD5Init(&ctx);
  MD5Digest digest;
  MD5Update(&ctx, content.data(), content.size());
  MD5Final(&digest, &ctx);
  return MD5DigestToBase16(digest);
}

}  // namespace net_instaweb
