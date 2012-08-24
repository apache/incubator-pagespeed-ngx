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
// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/javascript_library_identification.h"

#include <utility>
#include <map>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

JavascriptLibraryIdentification::~JavascriptLibraryIdentification() { }

bool JavascriptLibraryIdentification::RegisterLibrary(
    SizeInBytes bytes, StringPiece md5_hash, StringPiece canonical_url) {
  // Validate the md5_hash and make sure canonical_url is vaguely sensible.
  for (StringPiece::const_iterator c = md5_hash.begin(),
           end = md5_hash.end(); c != end; ++c) {
    if (!(('a' <= *c && *c <= 'z') || ('A' <= *c && *c <= 'Z') ||
          ('0' <= *c && *c <= '9') || (*c == '-') || (*c == '_'))) {
      return false;
    }
  }
  // Check url for basic validity by resolving it against example.com.
  GoogleUrl base("http://www.example.com/");
  GoogleUrl gurl(base, canonical_url);
  if (!gurl.is_valid()) {
    return false;
  }
  MD5ToUrlMap& bytes_entry = libraries_[bytes];  // Creates inner map if absent.
  GoogleString md5_hash_string;
  md5_hash.CopyToString(&md5_hash_string);
  GoogleString& url_string = bytes_entry[md5_hash_string];
  // New entry overrides any previous entry.
  canonical_url.CopyToString(&url_string);
  return true;
}

StringPiece JavascriptLibraryIdentification::Find(
    StringPiece minified_code) const {
  // NOTE: we are careful to avoid content hashing here unless the code size
  // suggests a possible match.
  uint64 bytes = minified_code.size();
  LibraryMap::const_iterator bytes_entry = libraries_.find(bytes);
  if (bytes_entry != libraries_.end()) {
    // Size match found, compute the hash and look up against all
    // appropriately-sized entries.
    MD5Hasher hasher;
    GoogleString hash = hasher.Hash(minified_code);
    const MD5ToUrlMap& md5_map = bytes_entry->second;
    MD5ToUrlMap::const_iterator url_entry = md5_map.find(hash);
    if (url_entry != md5_map.end()) {
      return StringPiece(url_entry->second);
    }
  }
  // No size match found or no hash matched
  return StringPiece(NULL);
}

void JavascriptLibraryIdentification::Merge(
    const JavascriptLibraryIdentification& src) {
  for (LibraryMap::const_iterator bytes_entry = src.libraries_.begin(),
           bytes_end = src.libraries_.end();
       bytes_entry != bytes_end; ++bytes_entry) {
    const MD5ToUrlMap& src_md5_map = bytes_entry->second;
    MD5ToUrlMap& this_md5_map = libraries_[bytes_entry->first];
    for (MD5ToUrlMap::const_iterator md5_entry = src_md5_map.begin(),
             md5_end = src_md5_map.end();
         md5_entry != md5_end; ++md5_entry) {
      // Prefer entry in src.
      this_md5_map[md5_entry->first] = md5_entry->second;
    }
  }
}

void JavascriptLibraryIdentification::AppendSignature(
    GoogleString* signature) const {
  for (LibraryMap::const_iterator bytes_entry = libraries_.begin(),
           bytes_end = libraries_.end();
       bytes_entry != bytes_end; ++bytes_entry) {
    StrAppend(signature, "S:", Integer64ToString(bytes_entry->first));
    const MD5ToUrlMap& md5_map = bytes_entry->second;
    for (MD5ToUrlMap::const_iterator md5_entry = md5_map.begin(),
             md5_end = md5_map.end();
         md5_entry != md5_end; ++md5_entry) {
      StrAppend(signature, "_H:", md5_entry->first,
                "_J:", md5_entry->second);
    }
  }
}

}  // namespace net_instaweb
