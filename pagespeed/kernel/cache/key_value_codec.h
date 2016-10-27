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

#ifndef PAGESPEED_KERNEL_CACHE_KEY_VALUE_CODEC_H_
#define PAGESPEED_KERNEL_CACHE_KEY_VALUE_CODEC_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class SharedString;

namespace key_value_codec {

// Takes a key and a value, and encodes the pair of them into key_and_value,
// sharing storage with value.
//
// Returns false if the key's size was too large (65535 max) to fit into the
// encoding.
bool Encode(StringPiece key, const SharedString& value,
            SharedString* key_and_value);

// Takes a combined key and a value, and decodes them into key and value,
// sharing the storage with key_and_value.
//
// Returns false if the key_and_value could not be successfully decoded, e.g.
// because it was corrupted or was not the result of Encode().
bool Decode(SharedString* key_and_value, GoogleString* key,
            SharedString* value);

// Given a key and the encoded key_and_value, returns the size of the value.
int GetValueSizeFromKeyAndKeyValue(StringPiece key,
                                   const SharedString& key_and_value);

}  // namespace key_value_codec

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_KEY_VALUE_CODEC_H_
