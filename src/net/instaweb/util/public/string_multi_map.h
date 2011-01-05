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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STRING_MULTI_MAP_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STRING_MULTI_MAP_H_

#include <stdlib.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Implements an ordered string map, providing case-sensitive and case
// insensitive versions.  The order of insertion is retained and
// names/value pairs can be accessed by index or looked up by name.
template<class StringCompare> class StringMultiMap {
 public:
  StringMultiMap() { }
  ~StringMultiMap() {
    Clear();
  }

  void Clear() {
    for (int i = 0, n = vector_.size(); i < n; ++i) {
      delete [] vector_[i].second;
    }
    map_.clear();
    vector_.clear();
  }

  // Returns the number of distinct names
  int num_names() const { return map_.size(); }

  // Returns the number of distinct values, which can be larger than num_names
  // if Add is called twice with the same name.
  int num_values() const { return vector_.size(); }

  // Find the value(s) associated with a variable.  Note that you may
  // specify a variable multiple times by calling Add multiple times
  // with the same variable, and each of these values will be returned
  // in the vector.
  bool Lookup(const char* name, CharStarVector* values) const {
    typename Map::const_iterator p = map_.find(name);
    bool ret = false;
    if (p != map_.end()) {
      ret = true;
      *values = p->second;
    }
    return ret;
  }

  // Remove all variables by name.
  void RemoveAll(const char* var_name) {
    StringPairVector temp_vector;  // Temp variable for new vector.
    temp_vector.reserve(vector_.size());
    for (int i = 0; i < num_values(); ++i) {
      if (strcasecmp(name(i),  var_name) != 0) {
        temp_vector.push_back(vector_[i]);
      } else {
        delete [] vector_[i].second;
      }
    }
    vector_.swap(temp_vector);

    // Note: we have to erase from the map second, because map owns the name.
    map_.erase(var_name);
  }

  const char* name(int index) const { return vector_[index].first; }

  // Note that the value can be NULL.
  const char* value(int index) const { return vector_[index].second; }

  // Add a new variable.  The value can be null.
  void Add(const StringPiece& var_name, const StringPiece& value) {
    CharStarVector dummy_values;
    std::string name_buf(var_name.data(), var_name.size());
    std::pair<typename Map::iterator, bool> iter_inserted = map_.insert(
        typename Map::value_type(name_buf.c_str(), dummy_values));
    typename Map::iterator iter = iter_inserted.first;
    CharStarVector& values = iter->second;
    char* value_copy = NULL;
    if (value.data() != NULL) {
      int value_size = value.size();
      value_copy = new char[value_size + 1];
      memcpy(value_copy, value.data(), value_size);
      value_copy[value_size] = '\0';
    }
    values.push_back(value_copy);
    vector_.push_back(StringPair(iter->first.c_str(), value_copy));
  }

 private:
  // We are keeping two structures, conceptually map<String,vector<String>> and
  // vector<pair<String,String>>, so we can do associative lookups and
  // also order-preserving iteration and easy indexed access.
  //
  // To avoid duplicating the strings, we will have the map own the
  // Names (keys) in a std::string, and the string-pair-vector own the
  // value as an explicitly newed char*.  The risk of using a std::string
  // to hold the value is that the pointers will not survive a resize.
  typedef std::pair<const char*, char*> StringPair;  // owns the value
  typedef std::map<std::string, CharStarVector, StringCompare> Map;
  typedef std::vector<StringPair> StringPairVector;

  Map map_;
  StringPairVector vector_;

  DISALLOW_COPY_AND_ASSIGN(StringMultiMap);
};

class StringMultiMapInsensitive
    : public StringMultiMap<StringCompareInsensitive> {
 public:
  StringMultiMapInsensitive() { }
 private:
  DISALLOW_COPY_AND_ASSIGN(StringMultiMapInsensitive);
};

class StringMultiMapSensitive : public StringMultiMap<StringCompareSensitive> {
 public:
  StringMultiMapSensitive() { }
 private:
  DISALLOW_COPY_AND_ASSIGN(StringMultiMapSensitive);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_MULTI_MAP_H_
