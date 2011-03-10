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
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_NAMER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_NAMER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ContentType;
class ResourceManager;

// Encapsulates the naming of resource URL leafs.  The class holds the context
// of a single resource, and is not intended for re-use.  We could, of course,
// add a Clear(), but it is a stateful class.
class ResourceNamer {
 public:
  // This determines the overhead imposed on each URL by the ResourceNamer
  // syntax, such as separators.
  static const int kOverhead;

  ResourceNamer() {}
  ~ResourceNamer() {}

  // Encoding and decoding in various formats.

  // Decodes an entire resource name (ID.HASH.NAME.EXT), placing
  // the result in the fields in this encoder.
  bool Decode(const StringPiece& encoded_string);

  // Encodes the fields in this encoder into an absolute url, with the
  // trailing portion "NAME.pagespeed.ID.HASH.EXT".
  std::string Encode() const;

  // Encode a key that can used to do a lookup based on an id
  // and the name.  This key can be used to find the hash-code for a
  // resource within the origin TTL.
  //
  // The 'id' is a short code indicating which Instaweb rewriter was
  // used to generate the resource.
  std::string EncodeIdName() const;

  // Note: there is no need at this time to decode the name key.

  // Simple getters
  StringPiece id() const { return id_; }
  StringPiece name() const { return name_; }
  StringPiece hash() const { return hash_; }
  StringPiece ext() const { return ext_; }

  // Simple setters
  void set_id(const StringPiece& p) { p.CopyToString(&id_); }
  void set_name(const StringPiece& n) { n.CopyToString(&name_); }
  void set_hash(const StringPiece& h) { h.CopyToString(&hash_); }
  void set_ext(const StringPiece& e) {
    // TODO(jmaessen): Remove check after transitioning to undotted extensions
    // everywhere.
    CHECK(e.empty() || e[0] != '.');
    e.CopyToString(&ext_);
  }

  // Other setter-like operations
  void ClearHash() { hash_.clear(); }
  void CopyFrom(const ResourceNamer& other);

  // Utility functions

  // Name suitable for debugging and logging
  std::string PrettyName() const {return  InternalEncode(); }

  // Compute a content-type based on ext().  NULL if unrecognized.
  const ContentType* ContentTypeFromExt() const;

 private:
  std::string InternalEncode() const;
  bool LegacyDecode(const StringPiece& encoded_string);

  std::string id_;
  std::string name_;
  std::string hash_;
  std::string ext_;

  DISALLOW_COPY_AND_ASSIGN(ResourceNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_NAMER_H_
