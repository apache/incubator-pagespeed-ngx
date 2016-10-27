/*
 * Copyright 2012 Google Inc.
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

#include "pagespeed/kernel/cache/key_value_codec.h"

#include <limits.h>                     // for CHAR_BIT
#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace {

// We can't store arbitrary keys in some caches (e.g. memcached), so
// encode the actual key in the value.  Thus in the unlikely event of
// a hash collision, we can reject the mismatched full key when
// reading.
//
// We encode the length as the last two bytes.  Keys of length greater than
// 65535 bytes result in an encode failure, and false returned.
const int kKeySizeOverheadBytes = 2;
const size_t kKeyMaxLength = (1 << (kKeySizeOverheadBytes * CHAR_BIT)) - 1;

}  // namespace

namespace net_instaweb {

namespace key_value_codec {

// Takes a key and a value, and encodes the pair of them into key_value,
// sharing storage with value.
//
// The encoding format is [value, key, 2 bytes of key size].
bool Encode(StringPiece key, const SharedString& value,
            SharedString* key_value) {
  if (key.size() > kKeyMaxLength) {
    return false;
  }
  uint32 key_size = key.size();
  *key_value = value;
  key_value->Append(key);
  uint8 ch = key_size & 0xff;
  key_value->Append(reinterpret_cast<char*>(&ch), 1);
  ch = (key_size >> 8) & 0xff;
  key_value->Append(reinterpret_cast<char*>(&ch), 1);
  return true;
}

// Takes a combined key and a value, and decodes them into key and value,
// sharing the storage with key_value.
bool Decode(SharedString* key_value, GoogleString* key, SharedString* value) {
  int key_value_size = key_value->size();
  if (key_value_size < kKeySizeOverheadBytes) {
    return false;
  }
  const uint8* data = reinterpret_cast<const uint8*>(key_value->data());
  int key_size = data[key_value_size - 1];
  key_size <<= 8;
  key_size |= data[key_value_size - 2];
  key_value_size -= kKeySizeOverheadBytes;  // ignore overhead now.
  if (key_value_size < key_size) {
    return false;
  }
  uint32 value_size = key_value_size - key_size;
  key->assign(reinterpret_cast<const char*>(data) + value_size, key_size);
  *value = *key_value;  // Shares string storage, but with different prefixes.
  value->RemoveSuffix(key_size + kKeySizeOverheadBytes);
  return true;
}

int GetValueSizeFromKeyAndKeyValue(StringPiece key,
                                   const SharedString& key_and_value) {
  return key_and_value.size() - key.size() - kKeySizeOverheadBytes;
}

}  // namespace key_value_codec

}  // namespace net_instaweb
