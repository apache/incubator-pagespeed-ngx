// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A JSON parser.  Converts strings of JSON into a Value object (see
// base/values.h).
// http://www.ietf.org/rfc/rfc4627.txt?number=4627
//
// Known limitations/deviations from the RFC:
// - Only knows how to parse ints within the range of a signed 32 bit int and
//   decimal numbers within a double.
// - Assumes input is encoded as UTF8.  The spec says we should allow UTF-16
//   (BE or LE) and UTF-32 (BE or LE) as well.
// - We limit nesting to 100 levels to prevent stack overflow (this is allowed
//   by the RFC).
// - A Unicode FAQ ("http://unicode.org/faq/utf_bom.html") writes a data
//   stream may start with a Unicode Byte-Order-Mark (U+FEFF), i.e. the input
//   UTF-8 string for the JSONReader::JsonToValue() function may start with a
//   UTF-8 BOM (0xEF, 0xBB, 0xBF).
//   To avoid the function from mis-treating a UTF-8 BOM as an invalid
//   character, the function skips a Unicode BOM at the beginning of the
//   Unicode string (converted from the input UTF-8 string) before parsing it.
//
// TODO(tc): Add a parsing option to to relax object keys being wrapped in
//   double quotes
// TODO(tc): Add an option to disable comment stripping
// TODO(aa): Consider making the constructor public and the static Read() method
// only a convenience for the common uses with more complex configuration going
// on the instance.

#ifndef BASE_JSON_JSON_READER_H_
#define BASE_JSON_JSON_READER_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"

// Chromium and Chromium OS check out gtest to different places, so we're
// unable to compile on both if we include gtest_prod.h here.  Instead, include
// its only contents -- this will need to be updated if the macro ever changes.
#define FRIEND_TEST(test_case_name, test_name)\
friend class test_case_name##_##test_name##_Test

class Value;

namespace base {

class BASE_API JSONReader {
 public:
  // A struct to hold a JS token.
  class Token {
   public:
    enum Type {
     OBJECT_BEGIN,           // {
     OBJECT_END,             // }
     ARRAY_BEGIN,            // [
     ARRAY_END,              // ]
     STRING,
     NUMBER,
     BOOL_TRUE,              // true
     BOOL_FALSE,             // false
     NULL_TOKEN,             // null
     LIST_SEPARATOR,         // ,
     OBJECT_PAIR_SEPARATOR,  // :
     END_OF_INPUT,
     INVALID_TOKEN,
    };
    Token(Type t, const wchar_t* b, int len)
      : type(t), begin(b), length(len) {}

    // Get the character that's one past the end of this token.
    wchar_t NextChar() {
      return *(begin + length);
    }

    Type type;

    // A pointer into JSONReader::json_pos_ that's the beginning of this token.
    const wchar_t* begin;

    // End should be one char past the end of the token.
    int length;
  };

  // Error codes during parsing.
  enum JsonParseError {
    JSON_NO_ERROR = 0,
    JSON_BAD_ROOT_ELEMENT_TYPE,
    JSON_INVALID_ESCAPE,
    JSON_SYNTAX_ERROR,
    JSON_TRAILING_COMMA,
    JSON_TOO_MUCH_NESTING,
    JSON_UNEXPECTED_DATA_AFTER_ROOT,
    JSON_UNSUPPORTED_ENCODING,
    JSON_UNQUOTED_DICTIONARY_KEY,
  };

  // String versions of parse error codes.
  static const char* kBadRootElementType;
  static const char* kInvalidEscape;
  static const char* kSyntaxError;
  static const char* kTrailingComma;
  static const char* kTooMuchNesting;
  static const char* kUnexpectedDataAfterRoot;
  static const char* kUnsupportedEncoding;
  static const char* kUnquotedDictionaryKey;

  JSONReader();

  // Reads and parses |json|, returning a Value. The caller owns the returned
  // instance. If |json| is not a properly formed JSON string, returns NULL.
  // If |allow_trailing_comma| is true, we will ignore trailing commas in
  // objects and arrays even though this goes against the RFC.
  static Value* Read(const std::string& json, bool allow_trailing_comma);

  // Reads and parses |json| like Read(). |error_code_out| and |error_msg_out|
  // are optional. If specified and NULL is returned, they will be populated
  // an error code and a formatted error message (including error location if
  // appropriate). Otherwise, they will be unmodified.
  static Value* ReadAndReturnError(const std::string& json,
                                   bool allow_trailing_comma,
                                   int* error_code_out,
                                   std::string* error_msg_out);

  // Converts a JSON parse error code into a human readable message.
  // Returns an empty string if error_code is JSON_NO_ERROR.
  static std::string ErrorCodeToString(JsonParseError error_code);

  // Returns the error code if the last call to JsonToValue() failed.
  // Returns JSON_NO_ERROR otherwise.
  JsonParseError error_code() const { return error_code_; }

  // Converts error_code_ to a human-readable string, including line and column
  // numbers if appropriate.
  std::string GetErrorMessage() const;

  // Reads and parses |json|, returning a Value. The caller owns the returned
  // instance. If |json| is not a properly formed JSON string, returns NULL and
  // a detailed error can be retrieved from |error_message()|.
  // If |check_root| is true, we require that the root object be an object or
  // array. Otherwise, it can be any valid JSON type.
  // If |allow_trailing_comma| is true, we will ignore trailing commas in
  // objects and arrays even though this goes against the RFC.
  Value* JsonToValue(const std::string& json, bool check_root,
                     bool allow_trailing_comma);

 private:
  FRIEND_TEST(JSONReaderTest, Reading);
  FRIEND_TEST(JSONReaderTest, ErrorMessages);

  static std::string FormatErrorMessage(int line, int column,
                                        const std::string& description);

  // Recursively build Value.  Returns NULL if we don't have a valid JSON
  // string.  If |is_root| is true, we verify that the root element is either
  // an object or an array.
  Value* BuildValue(bool is_root);

  // Parses a sequence of characters into a Token::NUMBER. If the sequence of
  // characters is not a valid number, returns a Token::INVALID_TOKEN. Note
  // that DecodeNumber is used to actually convert from a string to an
  // int/double.
  Token ParseNumberToken();

  // Try and convert the substring that token holds into an int or a double. If
  // we can (ie., no overflow), return the value, else return NULL.
  Value* DecodeNumber(const Token& token);

  // Parses a sequence of characters into a Token::STRING. If the sequence of
  // characters is not a valid string, returns a Token::INVALID_TOKEN. Note
  // that DecodeString is used to actually decode the escaped string into an
  // actual wstring.
  Token ParseStringToken();

  // Convert the substring into a value string.  This should always succeed
  // (otherwise ParseStringToken would have failed).
  Value* DecodeString(const Token& token);

  // Grabs the next token in the JSON stream.  This does not increment the
  // stream so it can be used to look ahead at the next token.
  Token ParseToken();

  // Increments |json_pos_| past leading whitespace and comments.
  void EatWhitespaceAndComments();

  // If |json_pos_| is at the start of a comment, eat it, otherwise, returns
  // false.
  bool EatComment();

  // Checks if |json_pos_| matches str.
  bool NextStringMatch(const std::wstring& str);

  // Sets the error code that will be returned to the caller. The current
  // line and column are determined and added into the final message.
  void SetErrorCode(const JsonParseError error, const wchar_t* error_pos);

  // Pointer to the starting position in the input string.
  const wchar_t* start_pos_;

  // Pointer to the current position in the input string.
  const wchar_t* json_pos_;

  // Used to keep track of how many nested lists/dicts there are.
  int stack_depth_;

  // A parser flag that allows trailing commas in objects and arrays.
  bool allow_trailing_comma_;

  // Contains the error code for the last call to JsonToValue(), if any.
  JsonParseError error_code_;
  int error_line_;
  int error_col_;

  DISALLOW_COPY_AND_ASSIGN(JSONReader);
};

}  // namespace base

#endif  // BASE_JSON_JSON_READER_H_
