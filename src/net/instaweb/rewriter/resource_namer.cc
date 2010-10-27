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

#include "net/instaweb/rewriter/public/resource_namer.h"

#include <vector>
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/string_hash.h"

namespace {

static const char kSeparatorString[] = ".";
static const char kSeparatorChar = kSeparatorString[0];

}  // namespace

namespace net_instaweb {

bool ResourceNamer::Decode(const ResourceManager* resource_manager,
                           const StringPiece& encoded_string) {
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

// This is used for legacy compatibility as we transition to the grand new
// world.
std::string ResourceNamer::InternalEncode() const {
  return StrCat(id_, kSeparatorString,
                hash_, kSeparatorString,
                name_, kSeparatorString,
                ext_);
}

// The current encoding assumes there are no dots in any of the components.
// This restriction may be relaxed in the future, but check it aggressively
// for now.
std::string ResourceNamer::AbsoluteUrl(
    const ResourceManager* resource_manager) const {
  CHECK_EQ(StringPiece::npos, id_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, name_.find(kSeparatorChar));
  CHECK(!hash_.empty());
  CHECK_EQ(StringPiece::npos, hash_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, ext_.find(kSeparatorChar));
  std::string url_prefix(resource_manager->UrlPrefixFor(*this));
  return StrCat(url_prefix, InternalEncode());
}

std::string ResourceNamer::Filename(
    const ResourceManager* resource_manager) const {
  CHECK_EQ(StringPiece::npos, id_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, name_.find(kSeparatorChar));
  CHECK(!hash_.empty());
  CHECK_EQ(StringPiece::npos, hash_.find(kSeparatorChar));
  CHECK_EQ(StringPiece::npos, ext_.find(kSeparatorChar));
  std::string filename;
  FilenameEncoder* encoder = resource_manager->filename_encoder();
  encoder->Encode(
      resource_manager->filename_prefix(), InternalEncode(), &filename);
  return filename;
}


std::string ResourceNamer::EncodeIdName() const {
  CHECK(id_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(name_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(id_, kSeparatorString, name_);
}

// Note: there is no need at this time to decode the name key.

std::string ResourceNamer::EncodeHashExt() const {
  CHECK(hash_.find(kSeparatorChar) == StringPiece::npos);
  CHECK(ext_.find(kSeparatorChar) == StringPiece::npos);
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

size_t ResourceNamer::Hash() const {
  size_t id_hash   = HashString(  id_.data(),   id_.size());
  size_t name_hash = HashString(name_.data(), name_.size());
  size_t hash_hash = HashString(hash_.data(), hash_.size());
  size_t ext_hash  = HashString( ext_.data(),  ext_.size());
  return
      JoinHash(JoinHash(JoinHash(id_hash, name_hash), hash_hash), ext_hash);
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
