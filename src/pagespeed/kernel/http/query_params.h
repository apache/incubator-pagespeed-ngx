// Copyright 2010-2011 Google Inc.
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

#ifndef PAGESPEED_KERNEL_HTTP_QUERY_PARAMS_H_
#define PAGESPEED_KERNEL_HTTP_QUERY_PARAMS_H_

#include "pagespeed/kernel/base/string_multi_map.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class GoogleUrl;

// Parses and rewrites URL query parameters.
class QueryParams {
 public:
  QueryParams() { }

  // Parse the query part of the given URL, for example "x=0&y=1&z=2".
  //
  // Note that a query param value can be NULL, indicating that the name was not
  // followed by a '='. So given "a=0&b&c=", the values will be {"0", NULL, ""}.
  void ParseFromUrl(const GoogleUrl& gurl);

  // Parse the given untrusted string containing just query parameters,
  // for example "x=0&y=1&z=2". This is a wrapper method that constructs
  // a GoogleUrl from a dummy URL with the given string appended as query
  // params, then calls ParseFromUrl. Accordingly, the given string will
  // be sanitized by the GoogleUrl constructor: anything after an embedded
  // '#' will be discarded, '\t's & '\n's & '\r's will be discarded, control
  // chars will be %-encoded, ' ' & '"' & '<' & '>' & DEL will be %-encoded,
  // and, when building for open source, which uses chromium's version of
  // url_canon, single-quote ("'") is also %-encoded.
  void ParseFromUntrustedString(StringPiece query_param_string);

  // Generates an escaped query-string.
  GoogleString ToEscapedString() const;

  int size() const { return map_.num_values(); }
  bool empty() const { return map_.empty(); }
  void Clear() { map_.Clear(); }

  // Find the value(s) associated with a variable.  Note that you may
  // specify a variable multiple times by adding it multiple times
  // with the same variable, and each of these values will be returned
  // in the vector.
  //
  // Any non alphanumerics besides "-_.~" will be %-encoded, the
  // Unescaped version will have those evaluted out.  See
  // http://en.wikipedia.org/wiki/Query_string#URL_encoding
  //
  // If you want to get back values with %-encodings as originally specified
  // in the query_string passed to Parse(), use the Escaped APIs.  If you want
  // to get those values back decoded, uses the Unescaped APIs.
  bool LookupEscaped(const StringPiece& name,
                     ConstStringStarVector* values) const {
    return map_.Lookup(name, values);
  }
  // Note: there is no LookupUnescaped currently.  If we did define one it
  // might have this API:
  //   bool LookupUnescaped(const StringPiece& name, StringVector* values) const
  // and we'd have to answer the question of what the semantics should be
  // if some of the values were successfully unescaped, and others not.

  // Looks up a single value.  Returns NULL if the name is not found or more
  // than one value is found.  The Escaped version will be %-encoded per
  // http://en.wikipedia.org/wiki/Query_string#URL_encoding, the
  // Unescaped version will have those evaluated out, e.g.
  // the escaped form "Hello%2c+World%21" corresponds to the unescaped
  // form "Hello, World!".
  const GoogleString* Lookup1Escaped(const StringPiece& name) const {
    return map_.Lookup1(name);
  }

  // Looks up a single value.  Returns false if the name is not found, more
  // than one value is found, or there is an error encountered when unescaping.
  // See the documentation for Lookup1Unescaped
  bool Lookup1Unescaped(const StringPiece& name,
                        GoogleString* escaped_value) const;

  bool Has(const StringPiece& name) const { return map_.Has(name); }

  // Remove all variables by name.  Returns true if anything was removed.
  bool RemoveAll(const StringPiece& key) { return map_.RemoveAll(key); }

  // Remove all variables by name.  Returns true if anything was removed.
  //
  // The 'names' vector must be sorted based on StringCompareSensitive.
  bool RemoveAllFromSortedArray(const StringPiece* names, int names_size) {
    return map_.RemoveAllFromSortedArray(names, names_size);
  }

  StringPiece name(int index) const { return map_.name(index); }

  // Returns an indexed value.  Note that the returned value can be NULL,
  // which indicates the query-param did not have an "=" sign, or it
  // might be an empty string, indicating the query-param had an "="
  // but no value after it.
  //
  // The return value is left in its original escaped form.
  const GoogleString* EscapedValue(int index) const {
    return map_.value(index);
  }

  // Sets *escaped_val to the unescaped value at an index.  Returns false
  // if there is no value (no "=" in the query param) or if the query
  // parameter could not be decoded.
  bool UnescapedValue(int index, GoogleString* escaped_val) const;

  // Add a new variable.  The value can be null.
  void AddEscaped(const StringPiece& key, const StringPiece& value) {
    return map_.Add(key, value);
  }

  void CopyFrom(const QueryParams& query_param) {
    map_.CopyFrom(query_param.map_);
  }

 private:
  StringMultiMapSensitive map_;

  DISALLOW_COPY_AND_ASSIGN(QueryParams);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_QUERY_PARAMS_H_
