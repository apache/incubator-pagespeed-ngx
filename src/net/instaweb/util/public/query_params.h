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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_

#include <map>
#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Parses and rewrites URL query parameters.
class QueryParams {
 public:
  QueryParams() {}
  ~QueryParams() { Clear(); }

  // Parse a query param string, e.g. x=0&y=1&z=2.  We expect the "?"
  // to be extracted (e.g. this string is the output of GURL::query().
  void Parse(const StringPiece& query_string);

  // Find the values associated with a variable.  Note that you may
  // specify a variable multiple times in a query-string, e.g.
  //    a=0&a=1&a=2
  bool Lookup(const char* name, CharStarVector* values) const;

  // Remove all variables by name.
  void RemoveAll(const char* name);

  // Remove all variables.
  void Clear();

  // Raw access for random access to variable name/value pairs.
  int size() const { return variable_vector_.size(); }
  const char* name(int index) const { return variable_vector_[index].first; }

  // Note that the value can be NULL, indicating that the variables
  // was not followed by a '='.  So given "a=0&b&c=", the values will
  // be {"0", NULL, ""}.
  const char* value(int index) const { return variable_vector_[index].second; }

  std::string ToString() const;

  // Add a new variable.
  void Add(const StringPiece& name, const StringPiece& value);

 private:
  // We are keeping two structures, conceptually map<String,vector<String>> and
  // vector<pair<String,String>>, so we can do associative lookups and
  // also order-preserving iteration and random access.
  //
  // To avoid duplicating the strings, we will have the map own the
  // Names (keys) in a std::string, and the string-pair-vector own the
  // value as an explicitly newed char*.  The risk of using a std::string
  // to hold the value is that the pointers will not survive a resize.
  typedef std::pair<const char*, char*> StringPair;  // owns the value
  typedef std::map<std::string, CharStarVector> VariableMap;
  typedef std::vector<StringPair> VariableVector;

  VariableMap variable_map_;
  VariableVector variable_vector_;

  DISALLOW_COPY_AND_ASSIGN(QueryParams);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_QUERY_PARAMS_H_
