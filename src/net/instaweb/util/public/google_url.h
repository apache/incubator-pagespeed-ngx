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
//         nforman@google.com  (Naomi Forman)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_

#include <cstddef>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"


#include "googleurl/src/gurl.h"

namespace net_instaweb {

class GoogleUrl {
 public:
  explicit GoogleUrl(const GURL& gurl);
  explicit GoogleUrl(const GoogleString& spec);
  explicit GoogleUrl(const StringPiece& sp);
  explicit GoogleUrl(const char *str);
  // The following three constructors create a new GoogleUrl by resolving the
  // String(Piece) against the base.
  GoogleUrl(const GoogleUrl& base, const GoogleString& str);
  GoogleUrl(const GoogleUrl& base, const StringPiece& sp);
  GoogleUrl(const GoogleUrl& base, const char *str);
  GoogleUrl();

  void Swap(GURL* gurl) { gurl_.Swap(gurl); }
  void Swap(GoogleUrl* google_url) { gurl_.Swap(&google_url->gurl_); }
  bool Reset(const StringPiece& new_url);
  bool Reset(const GoogleUrl& new_url);

  // Resets this URL to be invalid.
  void Clear();

  // Returns a new GoogleUrl that is identical to this one but with additional
  // query param.  Name and value should both be legal and already encoded.
  // This is a factory method that returns a pointer, the caller is responsible
  // for the management of the new object's memory (the caller owns the
  // pointer).
  GoogleUrl* CopyAndAddQueryParam(const StringPiece& name,
                                  const StringPiece& value);

  // For "http://a.com/b/c/d?e=f/g#r" returns "http://a.com/b/c/d"
  // Returns empty StringPiece for invalid url.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllExceptQuery() const;

  // For "http://a.com/b/c/d?e=f#r" returns "#r"
  // For "http://a.com/b/c/d?e=f#r1#r2" returns "#r1#r2"
  // Returns empty StringPiece for invalid url.
  // AllExceptQuery() + Query() + AllAfterQuery() = Spec() when url is valid
  // Different from Parsed.ref in the case of multiple "#"s after "?"
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllAfterQuery() const;

  // For "http://a.com/b/c/d?e=f/g" returns "http://a.com/b/c/",
  // including trailing slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllExceptLeaf() const;

  // For "http://a.com/b/c/d?e=f/g" returns "d?e=f/g", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafWithQuery() const;

  // For "http://a.com/b/c/d?e=f/g" returns "d", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafSansQuery() const;

  // For "http://a.com/b/c/d?E=f/g" returns "/b/c/d?e=f/g"
  // including leading slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathAndLeaf() const;

  // For "http://a.com/b/c/d/g.html" returns "/b/c/d/" including leading and
  // trailing slashes.
  // For queries, "http://a.com/b/c/d?E=f/g" returns "/b/c/".
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathSansLeaf() const;

  // Extracts the filename portion of the path and returns it. The filename
  // is everything after the last slash in the path. This may be empty.
  GoogleString ExtractFileName() const;

  StringPiece Host() const;

  // For "http://a.com/b/c/d?e=f/g returns "http://a.com"
  // without trailing slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece Origin() const;

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d" including leading slash,
  // and excluding the query.
  StringPiece PathSansQuery() const;

  StringPiece Query() const;

  // Returns scheme of stored url.
  StringPiece Scheme() const;

  // It is illegal to call this for invalid urls (i.e. check is_valid() first).
  StringPiece Spec() const;

  // Returns gurl_.spec_ without checking to see if it's valid or empty.
  StringPiece UncheckedSpec() const;

  // This method is primarily for printf purposes.
  const char* spec_c_str() const {
    return gurl_.possibly_invalid_spec().c_str();
  }

  int IntPort() const {
    return gurl_.IntPort();
  }

  // Returns the effective port number, which is dependent on the scheme.
  int EffectiveIntPort() const {
    return gurl_.EffectiveIntPort();
  }

  // Returns validity of stored url.
  bool is_valid() const {
    return gurl_.is_valid();
  }

  bool is_standard() const {
    return gurl_.IsStandard();
  }

  bool is_empty() const {
    return gurl_.is_empty();
  }

  bool has_scheme() const {
    return gurl_.has_scheme();
  }

  bool has_path() const {
    return gurl_.has_path();
  }

  bool has_query() const {
    return gurl_.has_query();
  }

  bool SchemeIs(const char* lower_ascii_scheme) const {
    return gurl_.SchemeIs(lower_ascii_scheme);
  }

  // TODO(nforman): get GURL to take a StringPiece so we don't have to do
  // any copying.
  bool SchemeIs(const StringPiece& lower_ascii_scheme) const {
    return gurl_.SchemeIs(lower_ascii_scheme.as_string().c_str());
  }

  // Defiant equality operator!
  bool operator==(const GoogleUrl& other) const {
    return gurl_ == other.gurl_;
  }
  bool operator!=(const GoogleUrl& other) const {
    return gurl_ != other.gurl_;
  }

 private:
  GURL gurl_;
  static size_t LeafEndPosition(const GURL &gurl);
  static size_t LeafStartPosition(const GURL &gurl);
  static size_t PathStartPosition(const GURL &gurl);
  size_t LeafEndPosition() const;
  size_t LeafStartPosition() const;
  size_t PathStartPosition() const;

  DISALLOW_COPY_AND_ASSIGN(GoogleUrl);
};  // class GoogleUrl

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_
