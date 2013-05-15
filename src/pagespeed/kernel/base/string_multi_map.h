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

#ifndef PAGESPEED_KERNEL_BASE_STRING_MULTI_MAP_H_
#define PAGESPEED_KERNEL_BASE_STRING_MULTI_MAP_H_

#include <map>
#include <utility>
#include <vector>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

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

  bool empty() {
    return vector_.empty();
  }

  void Clear() {
    for (int i = 0, n = vector_.size(); i < n; ++i) {
      delete vector_[i].second;
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
  bool Lookup(const StringPiece& name, ConstStringStarVector* values) const {
    typename Map::const_iterator p = map_.find(name.as_string());
    bool ret = false;
    if (p != map_.end()) {
      ret = true;
      *values = p->second;
    }
    return ret;
  }

  // Looks up a single value.  Returns NULL if the name is not found or more
  // than one value is found.
  const GoogleString* Lookup1(const StringPiece& name) const {
    ConstStringStarVector v;
    if (Lookup(name, &v) && v.size() == 1) {
      return v[0];
    }
    return NULL;
  }

  bool Has(const StringPiece& name) const {
    return map_.find(name.as_string()) != map_.end();
  }

  // Remove all variables by name.  Returns true if anything was removed.
  bool RemoveAll(const StringPiece& var_name) {
    GoogleString var_string(var_name.data(), var_name.size());
    typename Map::iterator p = map_.find(var_string);
    bool removed = (p != map_.end());
    if (removed) {
      StringPairVector temp_vector;  // Temp variable for new vector.
      temp_vector.reserve(vector_.size());
      for (int i = 0; i < num_values(); ++i) {
        if (!StringCaseEqual(name(i),  var_string)) {
          temp_vector.push_back(vector_[i]);
        } else {
          removed = true;
          delete vector_[i].second;
        }
      }

      vector_.swap(temp_vector);

      // Note: we have to erase from the map second, because map owns the name.
      map_.erase(p);
    }
    return removed;
  }

  const char* name(int index) const { return vector_[index].first; }

  // Note that the value can be NULL.
  const GoogleString* value(int index) const { return vector_[index].second; }

  // Add a new variable.  The value can be null.
  void Add(const StringPiece& var_name, const StringPiece& value) {
    ConstStringStarVector dummy_values;
    GoogleString name_buf(var_name.data(), var_name.size());
    std::pair<typename Map::iterator, bool> iter_inserted = map_.insert(
        typename Map::value_type(name_buf, dummy_values));
    typename Map::iterator iter = iter_inserted.first;
    ConstStringStarVector& values = iter->second;
    GoogleString* value_copy = NULL;
    if (value.data() != NULL) {
      value_copy = new GoogleString(value.as_string());
    }
    values.push_back(value_copy);
    vector_.push_back(StringPair(iter->first.c_str(), value_copy));
  }

  // Parse and add from a string of name-value pairs.
  // For example,
  //   "name1=value1,name2=value2,name3="
  // where separators is "," and value_separator is '='.
  // If omit_if_no_value is true, a name-value pair with an empty value will
  // not be added.
  void AddFromNameValuePairs(const StringPiece& name_value_list,
                             const StringPiece& separators,
                             char value_separator,
                             bool omit_if_no_value) {
    StringPieceVector pairs;
    SplitStringPieceToVector(name_value_list, separators, &pairs, true);
    for (int i = 0, n = pairs.size(); i < n; ++i) {
      StringPiece& pair = pairs[i];
      StringPiece::size_type pos = pair.find(value_separator);
      if (pos != StringPiece::npos) {
        Add(pair.substr(0, pos), pair.substr(pos + 1));
      } else if (!omit_if_no_value) {
        Add(pair, StringPiece(NULL, 0));
      }
    }
  }

  void CopyFrom(const StringMultiMap& string_multi_map) {
    Clear();
    for (int i = 0; i < string_multi_map.num_values(); ++i) {
      const GoogleString* value = string_multi_map.value(i);
      if (value != NULL) {
        Add(string_multi_map.name(i), *value);
      } else {
        Add(string_multi_map.name(i), NULL);
      }
    }
  }

 private:
  // We are keeping two structures, conceptually map<String,vector<String>> and
  // vector<pair<String,String>>, so we can do associative lookups and
  // also order-preserving iteration and easy indexed access.
  //
  // To avoid duplicating the strings, we will have the map own the
  // Names (keys) in a GoogleString, and the string-pair-vector own the
  // value as an explicitly newed char*.  The risk of using a GoogleString
  // to hold the value is that the pointers will not survive a resize.
  typedef std::pair<const char*, GoogleString*> StringPair;  // owns the value
  typedef std::map<GoogleString, ConstStringStarVector, StringCompare> Map;
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

#endif  // PAGESPEED_KERNEL_BASE_STRING_MULTI_MAP_H_
