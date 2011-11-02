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

#include "net/instaweb/http/public/headers.h"

#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/http.pb.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

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

template<class Proto> bool Headers<Proto>::IsCommaSeparatedField(
    const StringPiece& name) const {
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

template<class Proto> void Headers<Proto>::Add(
    const StringPiece& name, const StringPiece& value) {
  NameValue* name_value = proto_->add_header();
  name_value->set_name(name.data(), name.size());
  name_value->set_value(value.data(), value.size());
  AddToMap(name, value);
}

template<class Proto> void Headers<Proto>::AddToMap(
    const StringPiece& name, const StringPiece& value) const {
  if (map_.get() != NULL) {
    if (IsCommaSeparatedField(name)) {
      StringPieceVector split;
      SplitStringPieceToVector(value, ",", &split, true);
      for (int i = 0, n = split.size(); i < n; ++i) {
        StringPiece val = split[i];
        TrimWhitespace(&val);
        map_->Add(name, val);
      }
    } else {
      map_->Add(name, value);
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
      const char kSeparator[] = ", ";
      for (int i = 0, n = values.size(); i < n; ++i) {
        if (values[i] != NULL) {
          StringPiece val(*values[i]);
          if (i != val_index && !val.empty()) {
            if (concat) {
              StrAppend(&combined, val, kSeparator);
            } else {
              new_vals.push_back(val.as_string());
            }
          }
        }
      }
      RemoveAll(name);
      if (concat) {
        combined.erase(combined.length() - STATIC_STRLEN(kSeparator),
                       STATIC_STRLEN(kSeparator));
        Add(name, StringPiece(combined.data(), combined.size()));
      } else {
        for (int i = 0, n = new_vals.size(); i < n; ++i) {
          Add(name, new_vals[i]);
        }
      }
      return true;
    }
  }
  return false;
}

template<class Proto> bool Headers<Proto>::RemoveAll(const StringPiece& name) {
  StringSetInsensitive names;
  names.insert(name.as_string());
  return RemoveAllFromSet(names);
}

template<class Proto> bool Headers<Proto>::RemoveAllFromSet(
    const StringSetInsensitive& names) {
  // First, we update the map.
  PopulateMap();
  bool removed_anything = false;
  for (StringSetInsensitive::const_iterator iter = names.begin();
       iter != names.end(); ++iter) {
    if (map_->RemoveAll(*iter)) {
      removed_anything = true;
    }
  }

  // If we removed anything, we update the proto as well.
  if (removed_anything) {
    // Protobufs lack a convenient remove method for array elements, so
    // we construct a new protobuf and swap them.

    // Copy all headers that aren't slated for removal.
    Proto temp_proto;
    for (int i = 0, n = NumAttributes(); i < n; ++i) {
      if (names.find(Name(i)) == names.end()) {
        NameValue* name_value = temp_proto.add_header();
        name_value->set_name(Name(i));
        name_value->set_value(Value(i));
      }
    }

    // Copy back to our protobuf.
    proto_->clear_header();
    for (int i = 0, n = temp_proto.header_size(); i < n; ++i) {
      NameValue* name_value = proto_->add_header();
      name_value->set_name(temp_proto.header(i).name());
      name_value->set_value(temp_proto.header(i).value());
    }
  }

  return removed_anything;
}

template<class Proto> void Headers<Proto>::RemoveAllWithPrefix(
    const StringPiece& prefix) {
  // Protobufs lack a convenient remove method for array elements, so
  // we construct a new protobuf and swap them.

  // Copy all headers that aren't slated for removal.
  Proto temp_proto;
  for (int i = 0, n = NumAttributes(); i < n; ++i) {
    StringPiece name(Name(i));
    if (!StringCaseStartsWith(name, prefix)) {
      NameValue* name_value = temp_proto.add_header();
      name_value->set_name(Name(i));
      name_value->set_value(Value(i));
    }
  }

  // Copy back to our protobuf.
  map_.reset(NULL);  // Map must be repopulated before next lookup operation.
  proto_->clear_header();
  for (int i = 0, n = temp_proto.header_size(); i < n; ++i) {
    NameValue* name_value = proto_->add_header();
    name_value->set_name(temp_proto.header(i).name());
    name_value->set_value(temp_proto.header(i).value());
  }
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
  StringSetInsensitive removing_names;
  for (int i = 0, n = other.NumAttributes(); i < n; ++i) {
    removing_names.insert(other.Name(i));
  }
  // Remove them.
  RemoveAllFromSet(removing_names);
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

// Explicit template class instantiation.
// See http://www.cplusplus.com/forum/articles/14272/
template class Headers<HttpResponseHeaders>;
template class Headers<HttpRequestHeaders>;

}  // namespace net_instaweb
