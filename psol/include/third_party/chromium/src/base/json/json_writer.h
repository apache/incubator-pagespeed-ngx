// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_WRITER_H_
#define BASE_JSON_JSON_WRITER_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"

class Value;

namespace base {

class BASE_API JSONWriter {
 public:
  // Given a root node, generates a JSON string and puts it into |json|.
  // If |pretty_print| is true, return a slightly nicer formated json string
  // (pads with whitespace to help readability).  If |pretty_print| is false,
  // we try to generate as compact a string as possible.
  // TODO(tc): Should we generate json if it would be invalid json (e.g.,
  // |node| is not a DictionaryValue/ListValue or if there are inf/-inf float
  // values)?
  static void Write(const Value* const node, bool pretty_print,
                    std::string* json);

  // Same as above, but has an option to not escape the string, preserving its
  // UTF8 characters. It is useful if you can pass resulting string to the
  // JSON parser in binary form (as UTF8).
  static void WriteWithOptionalEscape(const Value* const node,
                                      bool pretty_print,
                                      bool escape,
                                      std::string* json);

  // A static, constant JSON string representing an empty array.  Useful
  // for empty JSON argument passing.
  static const char* kEmptyArray;

 private:
  JSONWriter(bool pretty_print, std::string* json);

  // Called recursively to build the JSON string.  Whe completed, value is
  // json_string_ will contain the JSON.
  void BuildJSONString(const Value* const node, int depth, bool escape);

  // Appends a quoted, escaped, version of (UTF-8) str to json_string_.
  void AppendQuotedString(const std::string& str);

  // Adds space to json_string_ for the indent level.
  void IndentLine(int depth);

  // Where we write JSON data as we generate it.
  std::string* json_string_;

  bool pretty_print_;

  DISALLOW_COPY_AND_ASSIGN(JSONWriter);
};

}  // namespace base

#endif  // BASE_JSON_JSON_WRITER_H_
