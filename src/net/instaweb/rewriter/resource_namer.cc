/*
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

#include "net/instaweb/rewriter/public/resource_namer.h"

#include <vector>
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/string_hash.h"

namespace net_instaweb {

namespace {

// The format of all resource names is:
//
//  ORIGINAL_NAME.ID.pagespeed.HASH.EXT
//
// "pagespeed" is what we'll call the system ID.  Rationale:
//   1. Any abbreviation of this will not be well known, e.g.
//         ps, mps (mod page speed), psa (page speed automatic)
//      and early reports from users indicate confusion over
//      the gibberish names in our resources.
//   2. "pagespeed" is the family of products now, not just the
//      firebug plug in.  Page Speed Automatic is the proper name for
//      the rewriting technology but it's longer, and "pagespeed" solves the
//      "WTF is this garbage in my URL" problem.
//   3. "mod_pagespeed" is slightly longer if/when this technology
//      is ported to other servers then the "mod_" is less relevant.
//
// If you change this, or the structure of the encoded string,
// you will also need to change:
//
// apache/install/system_test.sh
//
// Plus a few constants in _test.cc files.

static const char kSystemId[] = "pagespeed";
static const int kNumSegments = 5;
static const char kSeparatorString[] = ".";
static const char kSeparatorChar = kSeparatorString[0];

bool TokenizeSegmentFromRight(StringPiece* src, std::string* dest) {
  StringPiece::size_type pos = src->rfind(kSeparatorChar);
  if (pos == StringPiece::npos) {
    return false;
  }
  src->substr(pos + 1).CopyToString(dest);
  *src = src->substr(0, pos);
  return true;
}

}  // namespace

const int ResourceNamer::kOverhead = 4 + STATIC_STRLEN(kSystemId);

bool ResourceNamer::Decode(const StringPiece& encoded_string) {
  StringPiece src(encoded_string);
  std::string system_id;
  if (TokenizeSegmentFromRight(&src, &ext_) &&
      TokenizeSegmentFromRight(&src, &hash_) &&
      TokenizeSegmentFromRight(&src, &id_) &&
      TokenizeSegmentFromRight(&src, &system_id) &&
      (system_id == kSystemId)) {
    src.CopyToString(&name_);
    return true;
  }
  return LegacyDecode(encoded_string);
}

// TODO(jmarantz): validate that the 'id' is one of the filters that
// were implemented as of Nov 2010.  Also validate that the hash
// code is a 32-char hex number.
bool ResourceNamer::LegacyDecode(const StringPiece& encoded_string) {
  bool ret = false;
  // First check that this URL has a known extension type
  if (NameExtensionToContentType(encoded_string) != NULL) {
    std::vector<StringPiece> names;
    SplitStringPieceToVector(encoded_string, kSeparatorString, &names, true);
    if (names.size() == 4) {
      names[1].CopyToString(&hash_);

      // The legacy hash codes were all either 1-character (for tests) or
      // 32 characters, all in hex.
      if ((hash_.size() != 1) && (hash_.size() != 32)) {
        return false;
      }
      for (int i = 0, n = hash_.size(); i < n; ++i) {
        char ch = hash_[i];
        if (!isdigit(ch)) {
          ch = UpperChar(ch);
          if ((ch < 'A') || (ch > 'F')) {
            return false;
          }
        }
      }

      names[0].CopyToString(&id_);
      names[2].CopyToString(&name_);
      names[3].CopyToString(&ext_);
      ret = true;
    }
  }
  return ret;
}

// This is used for legacy compatibility as we transition to the grand new
// world.
std::string ResourceNamer::InternalEncode() const {
  return StrCat(name_, kSeparatorString,
                kSystemId, kSeparatorString,
                id_, kSeparatorString,
                StrCat(hash_, kSeparatorString, ext_));
}

// The current encoding assumes there are no dots in any of the components.
// This restriction may be relaxed in the future, but check it aggressively
// for now.
std::string ResourceNamer::Encode() const {
  CHECK_EQ(StringPiece::npos, id_.find(kSeparatorChar));
  CHECK(!hash_.empty());
  CHECK_EQ(StringPiece::npos, hash_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, ext_.find(kSeparatorChar));
 return InternalEncode();
}

std::string ResourceNamer::EncodeIdName() const {
  CHECK(id_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(id_, kSeparatorString, name_);
}

// Note: there is no need at this time to decode the name key.

std::string ResourceNamer::EncodeHashExt() const {
  CHECK_EQ(StringPiece::npos, hash_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, ext_.find(kSeparatorChar));
  return StrCat(hash_, kSeparatorString, ext_);
}

bool ResourceNamer::DecodeHashExt(const StringPiece& encoded_hash_ext) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(encoded_hash_ext, kSeparatorString, &names, true);
  bool ret = (names.size() == 2);
  if (ret) {
    names[0].CopyToString(&hash_);
    names[1].CopyToString(&ext_);
  }
  return ret;
}

const ContentType* ResourceNamer::ContentTypeFromExt() const {
  return NameExtensionToContentType(StrCat(".", ext_));
}

void ResourceNamer::CopyFrom(const ResourceNamer& other) {
  other.id().CopyToString(&id_);
  other.name().CopyToString(&name_);
  other.hash().CopyToString(&hash_);
  other.ext().CopyToString(&ext_);
}

}  // namespace net_instaweb
