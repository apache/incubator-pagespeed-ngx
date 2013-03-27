// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_
#define BASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/values.h"

class BASE_EXPORT JSONStringValueSerializer : public base::ValueSerializer {
 public:
  // json_string is the string that will be source of the deserialization
  // or the destination of the serialization.  The caller of the constructor
  // retains ownership of the string.
  explicit JSONStringValueSerializer(std::string* json_string)
      : json_string_(json_string),
        initialized_with_const_string_(false),
        pretty_print_(false),
        allow_trailing_comma_(false) {
  }

  // This version allows initialization with a const string reference for
  // deserialization only.
  explicit JSONStringValueSerializer(const std::string& json_string)
      : json_string_(&const_cast<std::string&>(json_string)),
        initialized_with_const_string_(true),
        pretty_print_(false),
        allow_trailing_comma_(false) {
  }

  virtual ~JSONStringValueSerializer();

  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the string passed into the constructor.
  virtual bool Serialize(const Value& root) OVERRIDE;

  // Equivalent to Serialize(root) except binary values are omitted from the
  // output.
  bool SerializeAndOmitBinaryValues(const Value& root);

  // Attempt to deserialize the data structure encoded in the string passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is NULL, and if |error_code| is non-null, |error_code| will
  // contain an integer error code (either JsonFileError or JsonParseError).
  // If |error_message| is non-null, it will be filled in with a formatted
  // error message including the location of the error if appropriate.
  // The caller takes ownership of the returned value.
  virtual Value* Deserialize(int* error_code,
                             std::string* error_message) OVERRIDE;

  void set_pretty_print(bool new_value) { pretty_print_ = new_value; }
  bool pretty_print() { return pretty_print_; }

  void set_allow_trailing_comma(bool new_value) {
    allow_trailing_comma_ = new_value;
  }

 private:
  bool SerializeInternal(const Value& root, bool omit_binary_values);

  std::string* json_string_;
  bool initialized_with_const_string_;
  bool pretty_print_;  // If true, serialization will span multiple lines.
  // If true, deserialization will allow trailing commas.
  bool allow_trailing_comma_;

  DISALLOW_COPY_AND_ASSIGN(JSONStringValueSerializer);
};

#endif  // BASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_

