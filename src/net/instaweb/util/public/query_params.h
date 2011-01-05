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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_

#include "net/instaweb/util/public/string_multi_map.h"

namespace net_instaweb {

// Parses and rewrites URL query parameters.
class QueryParams : public StringMultiMapSensitive {
 public:
  QueryParams() { }

  // Parse a query param string, e.g. x=0&y=1&z=2.  We expect the "?"
  // to be extracted (e.g. this string is the output of GURL::query().
  //
  // Note that the value can be NULL, indicating that the variables
  // was not followed by a '='.  So given "a=0&b&c=", the values will
  // be {"0", NULL, ""}.
  void Parse(const StringPiece& query_string);

  std::string ToString() const;

  int size() const { return num_values(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryParams);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_
