// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_WRITER_H_
#define BASE_JSON_JSON_WRITER_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {

class Value;

class BASE_EXPORT JSONWriter {
 public:
  enum Options {
    // Do not escape the string, preserving its UTF8 characters. It is useful
    // if you can pass the resulting string to the JSON parser in binary form
    // (as UTF8).
    OPTIONS_DO_NOT_ESCAPE = 1 << 0,

    // For values of binary type, the value (and key if within a dictionary)
    // will be omitted from the output.
    OPTIONS_OMIT_BINARY_VALUES = 1 << 1,

    // This option instructs the writer to write doubles that have no fractional
    // part as a normal integer (i.e., without using exponential notation
    // or appending a '.0') as long as the value is within the range of a
    // 64-bit int.
    OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION = 1 << 2,

    // Return a slightly nicer formatted json string (pads with whitespace to
    // help with readability).
    OPTIONS_PRETTY_PRINT = 1 << 3
  };

  // Given a root node, generates a JSON string and puts it into |json|.
  // TODO(tc): Should we generate json if it would be invalid json (e.g.,
  // |node| is not a DictionaryValue/ListValue or if there are inf/-inf float
  // values)?
  static void Write(const Value* const node, std::string* json);

  // Same as above but with |options| which is a bunch of JSONWriter::Options
  // bitwise ORed together.
  static void WriteWithOptions(const Value* const node, int options,
                               std::string* json);

  // A static, constant JSON string representing an empty array.  Useful
  // for empty JSON argument passing.
  static const char* kEmptyArray;

 private:
  JSONWriter(bool escape, bool omit_binary_values,
             bool omit_double_type_preservation, bool pretty_print,
             std::string* json);

  // Called recursively to build the JSON string.  Whe completed, value is
  // json_string_ will contain the JSON.
  void BuildJSONString(const Value* const node, int depth);

  // Appends a quoted, escaped, version of (UTF-8) str to json_string_.
  void AppendQuotedString(const std::string& str);

  // Adds space to json_string_ for the indent level.
  void IndentLine(int depth);

  bool escape_;
  bool omit_binary_values_;
  bool omit_double_type_preservation_;
  bool pretty_print_;

  // Where we write JSON data as we generate it.
  std::string* json_string_;

  DISALLOW_COPY_AND_ASSIGN(JSONWriter);
};

}  // namespace base

#endif  // BASE_JSON_JSON_WRITER_H_
