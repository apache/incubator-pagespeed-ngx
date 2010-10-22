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

#include "net/instaweb/util/public/query_params.h"

#include <stdio.h>
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

// TODO(jmarantz): Refactor much of this code with simple_meta_data.cc.  We'd
// have to templatize to indicate whether we wanted case sensitivity in our
// variables.

void QueryParams::Clear() {
  for (int i = 0, n = variable_vector_.size(); i < n; ++i) {
    delete [] variable_vector_[i].second;
  }
  variable_map_.clear();
  variable_vector_.clear();
}

bool QueryParams::Lookup(const char* name, CharStarVector* values) const {
  VariableMap::const_iterator p = variable_map_.find(name);
  bool ret = false;
  if (p != variable_map_.end()) {
    ret = true;
    *values = p->second;
  }
  return ret;
}

void QueryParams::Add(const StringPiece& name, const StringPiece& value) {
  CharStarVector dummy_values;
  std::string name_buf(name.data(), name.size());
  std::pair<VariableMap::iterator, bool> iter_inserted = variable_map_.insert(
      VariableMap::value_type(name_buf.c_str(), dummy_values));
  VariableMap::iterator iter = iter_inserted.first;
  CharStarVector& values = iter->second;
  char* value_copy = NULL;
  if (value.data() != NULL) {
    int value_size = value.size();
    value_copy = new char[value_size + 1];
    memcpy(value_copy, value.data(), value_size);
    value_copy[value_size] = '\0';
  }
  values.push_back(value_copy);
  variable_vector_.push_back(StringPair(iter->first.c_str(), value_copy));
}

void QueryParams::RemoveAll(const char* var_name) {
  VariableVector temp_vector;  // Temp variable for new vector.
  temp_vector.reserve(variable_vector_.size());
  for (int i = 0; i < size(); ++i) {
    if (strcasecmp(name(i),  var_name) != 0) {
      temp_vector.push_back(variable_vector_[i]);
    } else {
      delete [] variable_vector_[i].second;
    }
  }
  variable_vector_.swap(temp_vector);

  // Note: we have to erase from the map second, because map owns the name.
  variable_map_.erase(var_name);
}

void QueryParams::Parse(const StringPiece& text) {
  CHECK_EQ(0, size());
  std::vector<StringPiece> components;
  SplitStringPieceToVector(text, "&", &components, true);
  for (int i = 0, n = components.size(); i < n; ++i) {
    StringPiece::size_type pos = components[i].find('=');
    if (pos != StringPiece::npos) {
      Add(components[i].substr(0, pos), components[i].substr(pos + 1));
    } else {
      Add(components[i], StringPiece(NULL, 0));
    }
  }
}

std::string QueryParams::ToString() const {
  std::string str;
  const char* prefix="";
  for (int i = 0; i < size(); ++i) {
    if (value(i) == NULL) {
      str += StrCat(prefix, name(i));
    } else {
      str += StrCat(prefix, name(i), "=", value(i));
    }
    prefix = "&";
  }
  return str;
}

}  // namespace net_instaweb
