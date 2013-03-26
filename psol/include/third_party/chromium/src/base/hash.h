// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_H_
#define BASE_HASH_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {

// From http://www.azillionmonkeys.com/qed/hash.html
// This is the hash used on WebCore/platform/stringhash
BASE_EXPORT uint32 SuperFastHash(const char * data, int len);

inline uint32 Hash(const char* key, size_t length) {
  return SuperFastHash(key, static_cast<int>(length));
}

inline uint32 Hash(const std::string& key) {
  if (key.empty())
    return 0;
  return SuperFastHash(key.data(), static_cast<int>(key.size()));
}

}  // namespace base

#endif  // BASE_HASH_H_
