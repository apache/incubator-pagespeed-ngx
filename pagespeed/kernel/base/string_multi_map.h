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

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Implements an ordered string map, providing case-sensitive and case
// insensitive versions.  The order of insertion is retained and
// names/value pairs can be accessed by index or looked up by name.
//
// The keys and values in the map may contain embedded NUL characters.
// The values can also be the NULL pointer, which the API retains
// distinctly from empty strings.
template<class StringCompare> class StringMultiMap {
 public:
  StringMultiMap() { }
  ~StringMultiMap() {
    Clear();
  }

  bool empty() const {
    return vector_.empty();
  }

  void Clear() {
    for (int i = 0, n = vector_.size(); i < n; ++i) {
      delete vector_[i].second;
    }
    set_.clear();
    vector_.clear();
  }

  // Returns the number of distinct names
  int num_names() const { return set_.size(); }

  // Returns the number of distinct values, which can be larger than num_names
  // if Add is called twice with the same name.
  int num_values() const { return vector_.size(); }

  // Find the value(s) associated with a variable.  Note that you may
  // specify a variable multiple times by calling Add multiple times
  // with the same variable, and each of these values will be returned
  // in the vector.
  bool Lookup(const StringPiece& name, ConstStringStarVector* values) const {
    SetEntry lookup_entry(name);
    typename Set::const_iterator p = set_.find(lookup_entry);
    bool ret = false;
    if (p != set_.end()) {
      ret = true;
      const SetEntry& stored_entry = *p;
      *values = stored_entry.values();
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
    SetEntry lookup_entry(name);
    return set_.find(lookup_entry) != set_.end();
  }

  // Remove all variables by name.  Returns true if anything was removed.
  bool RemoveAll(const StringPiece& key) {
    return RemoveAllFromSortedArray(&key, 1);
  }

  // Remove all variables by name.  Returns true if anything was removed.
  //
  // The 'names' vector must be sorted based on StringCompare.
  bool RemoveAllFromSortedArray(const StringPiece* names, int names_size) {
#ifndef NDEBUG
    for (int i = 1; i < names_size; ++i) {
      StringCompare compare;
      // compare implements <, and we want <= to allow for for
      // duplicate entries, such as the two Set-Cookie entries that
      // occur in ResponseHeadersTest.TestUpdateFrom.  We could also
      // require call-sites to de-dup but this seems easier.  We can
      // implement a<=b via !compare(b, a).
      DCHECK(!compare(names[i], names[i - 1]))
          << "\"" << names[i - 1] << "\" vs \"" << names[i];
    }
#endif

    // Keep around dummy entry for stuffing in keys and doing lookups.
    // The values field for this instance is unused.
    SetEntry lookup_entry;

    // First, see if any of the names are in the map.  This way we'll avoid
    // making any allocations if there is no work to be done.  We cannot
    // actually remove the map entries, though, until we rebuild the vector,
    // since the map owns the StringPiece key storage used by the vector.
    const int kNotFound = -1;
    int index_of_first_match = kNotFound;
    typename Set::iterator set_entry_of_first_match;
    for (int i = 0; i < names_size; ++i) {
      lookup_entry.set_key(names[i]);
      set_entry_of_first_match = set_.find(lookup_entry);
      if (set_entry_of_first_match != set_.end()) {
        index_of_first_match = i;
        break;
      }
    }

    if (index_of_first_match == kNotFound) {
      return false;
    } else {
      StringPairVector temp_vector;  // Temp variable for new vector.
      temp_vector.reserve(vector_.size() - 1);
      StringCompare compare;
      for (int i = 0; i < num_values(); ++i) {
        if (std::binary_search(names, names + names_size, name(i), compare)) {
          delete vector_[i].second;
        } else {
          temp_vector.push_back(vector_[i]);
        }
      }

      vector_.swap(temp_vector);

      set_.erase(set_entry_of_first_match);
      for (int i = index_of_first_match + 1; i < names_size; ++i) {
        lookup_entry.set_key(names[i]);
        set_.erase(lookup_entry);
      }
    }
    return true;
  }

  StringPiece name(int index) const { return vector_[index].first; }

  // Note that the value can be NULL.
  const GoogleString* value(int index) const { return vector_[index].second; }

  // Add a new variable.  The value can be null.
  void Add(const StringPiece& key, const StringPiece& value) {
    SetEntry lookup_entry(key);
    std::pair<typename Set::iterator, bool> iter_inserted =
        set_.insert(lookup_entry);
    typename Set::iterator iter = iter_inserted.first;

    // To avoid letting you corrupt the comparison sanity of an STL set,
    // the 'insert' method returns an iterator that returns you only a
    // const reference to the stored entry.  However, the mutation we are
    // going to do here is to change the key to point to storage we'll
    // own in the entry, which we want to allocate only the first time
    // it is added.
    //
    // We also need to be able to mutate the entry to allow adding new
    // value entries.
    SetEntry& entry = const_cast<SetEntry&>(*iter);
    if (iter_inserted.second) {
      // The first time we insert, make a copy of the key in storage we own.
      entry.SaveKey();
    }
    GoogleString* value_copy = NULL;
    if (value.data() != NULL) {
      value_copy = new GoogleString(value.as_string());
    }
    entry.AddValue(value_copy);
    vector_.push_back(StringPair(iter->key(), value_copy));
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
        Add(string_multi_map.name(i), StringPiece());
      }
    }
  }

 private:
  // We are creating a map-like object using a std::set to make it clearer
  // how we are managing key storage within the entry.
  class SetEntry {
   public:
    SetEntry() { }
    SetEntry(StringPiece key) : key_(key) { }
    SetEntry(const SetEntry& src)
        : key_(src.key_) {
      // Note that a copy-construction does occur in Add, but only of
      // the lookup_entry, which will not have a saved key.
      DCHECK(src.values_.empty());
    }

    SetEntry& operator=(const SetEntry& src) {
      if (&src != this) {
        DCHECK(src.values_.empty());
        key_ = src.key_;
      }
      return *this;
    }

    void set_key(StringPiece key) {
      key_ = key;
    }

    void AddValue(const GoogleString* value) {
      values_.push_back(value);
    }

    // During lookups, key will point to the passed-in StringPiece.
    // However, the persistent entry we put in the map must duplicate
    // the key into key_storage and change key to point to that.
    void SaveKey() {
      key_.CopyToString(&key_storage_);
      key_ = key_storage_;
    }

    StringPiece key() const { return key_; }
    const ConstStringStarVector& values() const { return values_; }

   private:
    GoogleString key_storage_;
    StringPiece key_;
    ConstStringStarVector values_;
  };

  struct EntryCompare {
    bool operator()(const SetEntry& a, const SetEntry& b) const {
      return compare(a.key(), b.key());
    }

    StringCompare compare;
  };

  // We are keeping two structures, conceptually map<String,vector<String>> and
  // vector<pair<String,String>>, so we can do associative lookups and
  // also order-preserving iteration and easy indexed access.
  //
  // To avoid duplicating the strings and superfluous string-allocations on
  // lookups, we implement this via a set, whose entries own the keys.  A
  // separate string-pair-vector owns the values as new'd GoogleString*.  We
  // use a pointer here to avoid the cost of string-copies as the vector is
  // resized.
  typedef std::pair<StringPiece, GoogleString*> StringPair;  // owns the value
  typedef std::set<SetEntry, EntryCompare> Set;
  typedef std::vector<StringPair> StringPairVector;

  Set set_;
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
