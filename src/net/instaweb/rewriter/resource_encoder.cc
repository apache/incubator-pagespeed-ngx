/**
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/resource_encoder.h"
#include <vector>

namespace {

static const char kSeparatorString[] = ".";
static const char kSeparatorChar = kSeparatorString[0];

}  // namespace

namespace net_instaweb {

bool ResourceEncoder::Decode(const StringPiece& encoded_string) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(encoded_string, kSeparatorString, &names, true);
  bool ret = (names.size() == 4);
  if (ret) {
    names[0].CopyToString(&id_);
    names[1].CopyToString(&hash_);
    names[2].CopyToString(&name_);
    names[3].CopyToString(&ext_);
  }
  return ret;
}

// The current encoding assumes there are no dots in any of the components.
// This restriction may be relaxed in the future, but check it aggressively
// for now.
std::string ResourceEncoder::Encode() const {
  CHECK(id_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(name_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(hash_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(ext_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(id_, kSeparatorString,
                hash_, kSeparatorString,
                name_, kSeparatorString,
                ext_);
}

std::string ResourceEncoder::EncodeNameKey() const {
  CHECK(id_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(name_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(id_, kSeparatorString, name_);
}

// Note: there is no need at this time to decode the name key.

std::string ResourceEncoder::EncodeHashExt() const {
  CHECK(hash_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(ext_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(hash_, kSeparatorString, ext_);
}

bool ResourceEncoder::DecodeHashExt(const StringPiece& encoded_hash_ext) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(encoded_hash_ext, kSeparatorString, &names, true);
  bool ret = (names.size() == 2);
  if (ret) {
    names[0].CopyToString(&hash_);
    names[1].CopyToString(&ext_);
  }
  return ret;
}

}  // namespace net_instaweb
