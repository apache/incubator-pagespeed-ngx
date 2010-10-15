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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_ENCODER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_ENCODER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Encapsulates the encoding and decoding of resource URL leafs.  The class
// holds the context of the current leaf, and is not intended for re-use.  We
// could, of course, add a Clear(), but it is a stateful class.
class ResourceEncoder {
 public:
  ResourceEncoder() {}
  ~ResourceEncoder() {}

  // Decodes an entire resource name (ID.HASH.NAME.EXT), placing
  // the result in the fields in this encoder.
  bool Decode(const StringPiece& encoded_string);

  // Encodes the fields in this encoder into a resource name, in the
  // format "ID.HASH.NAME.EXT".
  std::string Encode() const;

  // Encode a key that can used to do a lookup based on an id
  // and the name.  This key can be used to find the hash-code for a
  // resource within the origin TTL.
  //
  // The 'id' is a short code indicating which Instaweb rewriter was
  // used to generate the resource.
  std::string EncodeIdName() const;

  // Note: there is no need at this time to decode the name key.

  // Encode/decode the hash and extension, which is used as the value
  // in the origin-TTL-bounded cache.
  std::string EncodeHashExt() const;
  bool DecodeHashExt(const StringPiece& encoded_hash_ext);

  void set_id(const StringPiece& p) { p.CopyToString(&id_); }
  void set_name(const StringPiece& n) { n.CopyToString(&name_); }
  void set_hash(const StringPiece& h) { h.CopyToString(&hash_); }
  void set_ext(const StringPiece& e) { e.CopyToString(&ext_); }

  StringPiece id() { return id_; }
  StringPiece name() { return name_; }
  StringPiece hash() { return hash_; }
  StringPiece ext() { return ext_; }

 private:
  std::string id_;
  std::string name_;
  std::string hash_;
  std::string ext_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_ENCODER_H_
