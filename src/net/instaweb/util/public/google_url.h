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

#include <string>
#include "net/instaweb/util/public/string_util.h"


#include "googleurl/src/gurl.h"

namespace net_instaweb {

class GoogleUrl {
 public:
  GoogleUrl(const GURL& gurl);
  GoogleUrl(const std::string& spec);
  GoogleUrl(const StringPiece& sp);
  GoogleUrl(const char *str);
  // The following three constructors create a new GoogleUrl by resolving the
  // String(Piece) against the base.
  GoogleUrl(const GoogleUrl& base, const std::string& str);
  GoogleUrl(const GoogleUrl& base, const StringPiece& sp);
  GoogleUrl(const GoogleUrl& base, const char *str);
  GoogleUrl();

  StringPiece Spec();

  // For "http://a.com/b/c/d?e=f/g returns "http://a/b/c/",
  // including trailing slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllExceptLeaf() const;

  // For "http://a.com/b/c/d?e=f/g returns "d?e=f/g", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafWithQuery() const;

  // For "http://a.com/b/c/d?e=f/g returns "d", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafSansQuery() const;

  // For "http://a.com/b/c/d?e=f/g returns "http://a.com"
  // without trailing slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece Origin() const;

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d?e=f/g"
  // including leading slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathAndLeaf() const;

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d" including leading slash
  StringPiece Path() const;

  // For "http://a.com/b/c/d/g.html returns "/b/c/d/" including leading and
  // trailing slashes.
  // For queries, "http://a.com/b/c/d?E=f/g" returns "/b/c/".
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathSansLeaf() const;

  // Returns scheme of stored url.
  StringPiece Scheme() const;

  // Returns validity of stored url.
  bool IsValid() const {
    return gurl_.is_valid();
  }

  const GURL& gurl() const { return gurl_; }

  bool IsStandard() const {
    return gurl_.IsStandard();
  }

  void Swap(GURL* gurl) { gurl_.Swap(gurl); }

  std::string GetUncheckedSpec() const {
    return gurl_.possibly_invalid_spec();
  }

  bool SchemeIs(const char* lower_ascii_scheme) const {
    return gurl_.SchemeIs(lower_ascii_scheme);
  }

  // TODO get GURL to take a StringPiece so we don't have to do
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
  // Helper functions around GURL to make it a little easier to use
  inline static std::string Spec(const GURL& gurl) { return gurl.spec(); }

  // Makes a GURL object from a StringPiece.
  inline static GURL Create(const StringPiece& sp) { return GURL(sp.as_string()); }

  // Makes a GURL object from a string.
  inline static GURL Create(const std::string& str) {
    return GURL(str);
  }

  // Resolves a GURL object and a new path into a new GURL.
  inline static GURL Resolve(const GURL& gurl, const StringPiece& sp) {
    return gurl.Resolve(sp.as_string()); }

  // Resolves a GURL object and a new path into a new GURL.
  inline static GURL Resolve(const GURL& gurl, const std::string& str) {
    return gurl.Resolve(str);
  }

  // Resolves a GURL object and a new path into a new GURL.
  inline static GURL Resolve(const GURL& gurl, const char* str) {
    return gurl.Resolve(str);
  }

  // For "http://a.com/b/c/d?e=f/g returns "http://a/b/c/",
  // including trailing slash.
  static std::string AllExceptLeaf(const GURL& gurl);

  // For "http://a.com/b/c/d?e=f/g returns "d?e=f/g", omitting leading slash.
  static std::string LeafWithQuery(const GURL& gurl);

  // For "http://a.com/b/c/d?e=f/g returns "d", omitting leading slash.
  static std::string LeafSansQuery(const GURL& gurl);

  // For "http://a.com/b/c/d?e=f/g returns "http://a.com" without trailing slash
  static std::string Origin(const GURL& gurl);

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d?e=f/g" including leading slash
  static std::string PathAndLeaf(const GURL& gurl);

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d" including leading slash
  inline static std::string Path(const GURL& gurl) {
    return gurl.path();
  }

  // For "http://a.com/b/c/d/g.html returns "/b/c/d/" including leading and
  // trailing slashes.
  // For queries, "http://a.com/b/c/d?E=f/g" returns "/b/c/".
  static std::string PathSansLeaf(const GURL& gurl);

 private:
  GURL gurl_;
  static size_t LeafStartPosition(const GURL &gurl);
  static size_t PathStartPosition(const GURL &gurl);
  size_t LeafStartPosition() const;
  size_t PathStartPosition() const;

};  // class GoogleUrl

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_
