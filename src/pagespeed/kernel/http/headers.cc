// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/http/headers.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_multi_map.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/http.pb.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

namespace {

// Helper function that removes a specific cookie from a cookie header
// and returns the new cookie header in new_cookie_header.
// For example: If cookie_header="A=1; VICTIM=2; B=3\r\n",
//              and cookie_name="VICTIM", then
//              new_cookie_header="A=1; B=3\r\n"
// Returns true if the cookie was found.
// Returns false otherwise, but still fills in new_cookie_header.
bool RemoveCookieString(const StringPiece& cookie_name,
                        const StringPiece& cookie_header,
                        GoogleString* new_cookie_header) {
  bool cookie_found = false;
  StringPieceVector pieces;
  SplitStringPieceToVector(cookie_header, ";", &pieces, false);
  GoogleString cookie_prefix(cookie_name.data(), cookie_name.size());
  cookie_prefix.append("=");
  for (int i = 0, n = pieces.size(); i < n; ++i) {
    StringPiece working_cookie = pieces[i];
    TrimLeadingWhitespace(&working_cookie);
    if (StringCaseStartsWith(working_cookie, cookie_prefix)) {
      cookie_found = true;
    } else {
      // Don't trim the whitespace off the cookie, just in case it actually
      // meant something.
      if (!pieces[i].empty()) {
        if (!new_cookie_header->empty()) {
          new_cookie_header->append(";");
        } else {
          // For the first cookie, trim the whitespace off the front.
          TrimLeadingWhitespace(&pieces[i]);
        }
        pieces[i].AppendToString(new_cookie_header);
      }
    }
  }
  return cookie_found;
}

// Helper that removes unneeded values from a headers proto array, without
// changing the order of items kept.
bool RemoveUnneeded(const std::vector<bool>& needed,
                    protobuf::RepeatedPtrField<NameValue>* headers) {
  CHECK_EQ(static_cast<size_t>(headers->size()), needed.size());

  int in = 0;
  int out = 0;
  int size = headers->size();
  // Invariant: [0, out) are all values we need.
  //            [out, in) are all values we don't need.
  //            [in, size) are unknown.
  while (in < size) {
    if (needed[in]) {
      // If in == out:
      //    The swap is is a no-op, and we grow [0, out),
      //    shrink [in, size) by 1 from left, and keep [out, in) as empty
      //    in between
      // If in != out:
      //    The valid region gets grown by one new entry, the invalid one is
      //    shifted one to the right.
      headers->SwapElements(in, out);
      ++in;
      ++out;
    } else {
      ++in;
    }
  }

  bool ret = (size != out);
  while (size != out) {
    headers->RemoveLast();
    --size;
  }
  return ret;
}

}  // namespace

class MessageHandler;

template<class Proto> Headers<Proto>::Headers() {
  proto_.reset(new Proto);
  Clear();
}

template<class Proto> Headers<Proto>::~Headers() {
  Clear();
}

template<class Proto> void Headers<Proto>::Clear() {
  proto_->clear_major_version();
  proto_->clear_minor_version();
  map_.reset(NULL);
}

template<class Proto> int Headers<Proto>::major_version() const {
  return proto_->major_version();
}

template<class Proto> bool Headers<Proto>::has_major_version() const {
  return proto_->has_major_version();
}

template<class Proto> int Headers<Proto>::minor_version() const {
  return proto_->minor_version();
}

template<class Proto> void Headers<Proto>::set_major_version(
    int major_version) {
  proto_->set_major_version(major_version);
}

template<class Proto> void Headers<Proto>::set_minor_version(
    int minor_version) {
  proto_->set_minor_version(minor_version);
}

template<class Proto> int Headers<Proto>::NumAttributes() const {
  return proto_->header_size();
}

template<class Proto> const GoogleString& Headers<Proto>::Name(int i) const {
  return proto_->header(i).name();
}

template<class Proto> const GoogleString& Headers<Proto>::Value(int i) const {
  return proto_->header(i).value();
}

template<class Proto> void Headers<Proto>::PopulateMap() const {
  if (map_.get() == NULL) {
    map_.reset(new StringMultiMapInsensitive);
    for (int i = 0, n = NumAttributes(); i < n; ++i) {
      AddToMap(Name(i), Value(i));
    }
  }
}

template<class Proto> int Headers<Proto>::NumAttributeNames() const {
  PopulateMap();
  return map_->num_names();
}

template<class Proto> bool Headers<Proto>::Lookup(
    const StringPiece& name, ConstStringStarVector* values) const {
  PopulateMap();
  return map_->Lookup(name, values);
}

template<class Proto> const char* Headers<Proto>::Lookup1(
    const StringPiece& name) const {
  ConstStringStarVector v;
  if (Lookup(name, &v) && (v.size() == 1)) {
    return v[0]->c_str();
  }
  return NULL;
}

template<class Proto> bool Headers<Proto>::Has(const StringPiece& name) const {
  PopulateMap();
  return map_->Has(name);
}

template<class Proto> bool Headers<Proto>::HasValue(
    const StringPiece& name, const StringPiece& value) const {
  ConstStringStarVector values;
  Lookup(name, &values);
  for (ConstStringStarVector::const_iterator iter = values.begin();
       iter != values.end(); ++iter) {
    if (value == **iter) {
      return true;
    }
  }
  return false;
}

namespace {

bool IsCommaSeparatedField(const StringPiece& name) {
  // TODO(nforman): Make this a complete list.  The list of header names
  // that are not safe to comma-split is at
  // http://src.chromium.org/viewvc/chrome/trunk/src/net/http/http_util.cc
  // (search for IsNonCoalescingHeader)
  if (StringCaseEqual(name, HttpAttributes::kVary) ||
      StringCaseEqual(name, HttpAttributes::kCacheControl) ||
      StringCaseEqual(name, HttpAttributes::kContentEncoding)) {
    return true;
  } else {
    return false;
  }
}

// Takes a potentially comma-separated value list, and splits it into
// a vector.  If the value is not comma-separatable, 'values' is
// populated with the single value.
void SplitValues(StringPiece name, StringPiece comma_separated_values,
                 StringPieceVector* values) {
  if (IsCommaSeparatedField(name)) {
    SplitStringPieceToVector(comma_separated_values, ",", values, true);
    if (values->empty()) {
      values->push_back(comma_separated_values);
    } else {
      for (int i = 0, n = values->size(); i < n; ++i) {
        TrimWhitespace(&((*values)[i]));
      }
    }
  } else {
    values->push_back(comma_separated_values);
  }
}

}  // namespace

template<class Proto> void Headers<Proto>::Add(
    const StringPiece& name, const StringPiece& value) {
  NameValue* name_value = proto_->add_header();
  name_value->set_name(name.data(), name.size());
  name_value->set_value(value.data(), value.size());
  AddToMap(name, value);
  UpdateHook();
}

template<class Proto> void Headers<Proto>::AddToMap(
    const StringPiece& name, const StringPiece& value) const {
  if (map_.get() != NULL) {
    StringPieceVector split;
    SplitValues(name, value, &split);
    for (int i = 0, n = split.size(); i < n; ++i) {
      map_->Add(name, split[i]);
    }
  }
}

template<class Proto> void Headers<Proto>::RemoveCookie(
    const StringPiece& cookie_name) {
  ConstStringStarVector values;
  if (Lookup(HttpAttributes::kCookie, &values)) {
    StringVector new_cookie_lines;
    bool remove_cookie = false;
    for (int i = 0, n = values.size(); i < n; ++i) {
      StringPiece cookie_header = *values[i];
      new_cookie_lines.push_back(GoogleString());
      bool removed = RemoveCookieString(cookie_name, cookie_header,
                                        &new_cookie_lines[i]);
      remove_cookie |= removed;
    }

    if (remove_cookie) {
      RemoveAll(HttpAttributes::kCookie);
      for (int i = 0, n = new_cookie_lines.size(); i < n; ++i) {
        if (!new_cookie_lines[i].empty()) {
          Add(HttpAttributes::kCookie,
              new_cookie_lines[i]);
        }
      }
    }
  }
}

// Remove works in a perverted manner.
// First comb through the values, from back to front, looking for the last
// instance of 'value'.
// Then remove all the values for name.
// Then add back in the ones that were not the 'value'.
// The string manipulation makes this horrendous, but hopefully no one has
// listed a header with 100 (or more) values.
template<class Proto> bool Headers<Proto>::Remove(const StringPiece& name,
                                                  const StringPiece& value) {
  PopulateMap();
  ConstStringStarVector values;
  bool found = map_->Lookup(name, &values);
  if (found) {
    int val_index = -1;
    for (int i = values.size() - 1; i >= 0; --i) {
      if (values[i] != NULL) {
        if (StringCaseEqual(*values[i], value)) {
          val_index = i;
          break;
        }
      }
    }
    if (val_index != -1) {
      StringVector new_vals;
      bool concat = IsCommaSeparatedField(name);
      GoogleString combined;
      StringPiece separator("", 0);  // change to "," after first entry.
      for (int i = 0, n = values.size(); i < n; ++i) {
        if (values[i] != NULL) {
          StringPiece val(*values[i]);
          if (i != val_index && !val.empty()) {
            if (concat) {
              StrAppend(&combined, separator, val);
              separator = ", ";
            } else {
              new_vals.push_back(val.as_string());
            }
          }
        }
      }
      RemoveAll(name);
      if (concat) {
        if (!combined.empty()) {
          Add(name, StringPiece(combined.data(), combined.size()));
        }
      } else {
        for (int i = 0, n = new_vals.size(); i < n; ++i) {
          Add(name, new_vals[i]);
        }
      }
      UpdateHook();
      return true;
    }
  }
  return false;
}

template<class Proto> bool Headers<Proto>::RemoveAll(const StringPiece& name) {
  return RemoveAllFromSortedArray(&name, 1);
}

template<class Proto> bool Headers<Proto>::RemoveAllFromSortedArray(
    const StringPiece* names, int names_size) {
  // First, we update the map.
  PopulateMap();
  bool removed_anything = map_->RemoveAllFromSortedArray(names, names_size);

  // If we removed anything, we update the proto as well.
  if (removed_anything) {
    // Remove all headers that are slated for removal.
    protobuf::RepeatedPtrField<NameValue>* headers = proto_->mutable_header();
    // Note: you might be tempted to consider repopulating the protobuf
    // from the map, which should be correct at this point, rather
    // than doing more searches.  This is feasible, but will result in
    // splitting multi-value entries into separate headers, e.g.
    // Cache-Control:max-age=0, no-cache will become two separate Cache-Control
    // entries, which will cause
    // ResponseHeadersTest.TestRemoveDoesntSeparateCommaValues to fail.
    // headers->Clear();
    // for (int i = 0; i < map_->num_values(); ++i) {
    //   NameValue* name_value = proto_->add_header();
    //   name_value->set_name(map_->name(i));
    //   const GoogleString* value = map_->value(i);
    //   name_value->set_value(value->data(), value->size());
    // }
    //
    // Instead, we call this helper, which re-implements StringMultiMap
    // functionality in the protobuf.
    RemoveFromHeaders(names, names_size, headers);
    UpdateHook();
  }

  return removed_anything;
}

template<class Proto> bool Headers<Proto>::RemoveFromHeaders(
    const StringPiece* names, int names_size,
    protobuf::RepeatedPtrField<NameValue>* headers) {
  // Remove all headers that are slated for removal.
  std::vector<bool> to_keep;
  to_keep.reserve(headers->size());

  StringCompareInsensitive compare;
  for (int i = 0, n = headers->size(); i < n; ++i) {
    to_keep.push_back(!std::binary_search(
        names, names + names_size, headers->Get(i).name(), compare));
  }
  return RemoveUnneeded(to_keep, headers);
}

template<class Proto> bool Headers<Proto>::RemoveAllWithPrefix(
    const StringPiece& prefix) {
  protobuf::RepeatedPtrField<NameValue>* headers = proto_->mutable_header();
  std::vector<bool> to_keep;
  to_keep.reserve(headers->size());

  for (int i = 0, n = headers->size(); i < n; ++i) {
    to_keep.push_back(!StringCaseStartsWith(headers->Get(i).name(), prefix));
  }
  bool ret = RemoveUnneeded(to_keep, headers);
  if (ret) {
    map_.reset(NULL);  // Map must be repopulated before next lookup operation.
    UpdateHook();
  }
  return ret;
}

template<class Proto> bool Headers<Proto>::RemoveIfNotIn(const Headers& keep) {
  // There are two removal scenarios: removing every value for a header, and
  // leaving some behind.  We don't use this->map_ to do this operation, but
  // we must invalidate the map if we make any mutations.  However we do use
  // keep.map_ via calls to keep.Lookup(name).
  //
  // Keep in mind also, that names may appear multiple times in the protobuf,
  // each with multiple values.  Typically Set-Cookie appears multiple times
  // with different values, and Cache-Control can appear one or more times, each
  // with comma-separated values.

  // First we loop through the protobuf, determining which elements to keep,
  // and executing any partial removals.
  std::vector<bool> to_keep;
  bool ret = false;
  typedef std::map<StringPiece, int> StringPieceBag;
  typedef std::map<StringPiece, StringPieceBag> ValueBagMap;
  ValueBagMap value_bag_map;

  for (int a = 0, na = NumAttributes(); a < na; ++a) {
    const GoogleString& name = Name(a);

    // Use a bag to keep track of all the occurrences of 'name' in 'keep'.
    // Note that we can encounter the same 'name' in multiple iterations of
    // the loop and we only want to use each occurence in 'keep' once.  So
    // we capture the occurrences in a name to bag that we collect as we
    // iterate, rather than going back to 'keep' each time.
    std::pair<ValueBagMap::iterator, bool> iter_found =
        value_bag_map.insert(ValueBagMap::value_type(name, StringPieceBag()));
    StringPieceBag& keep_value_bag = iter_found.first->second;
    if (iter_found.second) {  // 1st time we encounter a name, populate the bag.
      ConstStringStarVector keep_value_vector;
      if (keep.Lookup(name, &keep_value_vector)) {
        // Collect keep's values for name in a bag so we can do fast matches
        // and prune the bag to avoid matching a value more than once.
        for (int v = 0, nv = keep_value_vector.size(); v < nv; ++v) {
          StringPiece value = *keep_value_vector[v];
          int& value_count = keep_value_bag[value];
          ++value_count;
        }
      }
    }

    bool needed = false;
    if (!keep_value_bag.empty()) {
      StringPieceVector this_values;
      SplitValues(name, Value(a), &this_values);
      bool partial = false;
      int out = 0;
      for (int in = 0, nv = this_values.size(); in < nv; ++in) {
        StringPieceBag::iterator p = keep_value_bag.find(this_values[in]);
        if (p != keep_value_bag.end()) {
          int& value_count = p->second;
          if (--value_count == 0) {
            keep_value_bag.erase(p);
          }
          needed = true;
          if (in != out) {
            std::swap(this_values[in], this_values[out]);
          }
          ++out;
        } else {
          partial = true;
        }
      }
      if (needed && partial) {
        this_values.resize(out);
        GoogleString new_value = JoinCollection(this_values, ", ");
        proto_->mutable_header(a)->set_value(new_value);
        ret = true;
      }
    }
    to_keep.push_back(needed);
  }

  // Next we remove any protobuf entries with no matching values.
  ret |= RemoveUnneeded(to_keep, proto_->mutable_header());

  // Finally, if we did any mutations, clear the map_.  We didn't use this->map_
  // to execute the removals, but we may have invalidated it.
  if (ret) {
    map_.reset(NULL);
    UpdateHook();
  }
  return ret;
}

template<class Proto> void Headers<Proto>::Replace(
    const StringPiece& name, const StringPiece& value) {
  // TODO(jmarantz): This could be arguably be implemented more efficiently.
  RemoveAll(name);
  Add(name, value);
}

template<class Proto> void Headers<Proto>::UpdateFrom(
    const Headers<Proto>& other) {
  // Get set of names to remove.
  int n = other.NumAttributes();
  scoped_array<StringPiece> removing_names(new StringPiece[n]);
  for (int i = 0; i < n; ++i) {
    removing_names[i] = other.Name(i);
  }
  std::sort(removing_names.get(), removing_names.get() + n,
            StringCompareInsensitive());

  // Remove them.
  RemoveAllFromSortedArray(removing_names.get(), n);
  // Add new values.
  for (int i = 0, n = other.NumAttributes(); i < n; ++i) {
    Add(other.Name(i), other.Value(i));
  }
}

template<class Proto> bool Headers<Proto>::WriteAsBinary(
    Writer* writer, MessageHandler* handler) {
  GoogleString buf;
  {
    StringOutputStream sstream(&buf);
    proto_->SerializeToZeroCopyStream(&sstream);
  }
  return writer->Write(buf, handler);
}

template<class Proto> bool Headers<Proto>::ReadFromBinary(
    const StringPiece& buf, MessageHandler* message_handler) {
  Clear();
  ArrayInputStream input(buf.data(), buf.size());
  return proto_->ParseFromZeroCopyStream(&input);
}

template<class Proto> bool Headers<Proto>::WriteAsHttp(
    Writer* writer, MessageHandler* handler) const {
  bool ret = true;
  for (int i = 0, n = NumAttributes(); ret && (i < n); ++i) {
    ret &= writer->Write(Name(i), handler);
    ret &= writer->Write(": ", handler);
    ret &= writer->Write(Value(i), handler);
    ret &= writer->Write("\r\n", handler);
  }
  ret &= writer->Write("\r\n", handler);
  return ret;
}

template<class Proto>
void Headers<Proto>::CopyToProto(Proto* proto) {
  proto->CopyFrom(*proto_);
}

template<class Proto>
void Headers<Proto>::UpdateHook() {
}

// Explicit template class instantiation.
// See http://www.cplusplus.com/forum/articles/14272/
template class Headers<HttpResponseHeaders>;
template class Headers<HttpRequestHeaders>;

}  // namespace net_instaweb
