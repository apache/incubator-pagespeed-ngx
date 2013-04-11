// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_VALUES_TEST_UTIL_H_
#define BASE_TEST_VALUES_TEST_UTIL_H_

#include <string>

namespace base {
class DictionaryValue;
class ListValue;
class StringValue;

// All the functions below expect that the value for the given key in
// the given dictionary equals the given expected value.

void ExpectDictBooleanValue(bool expected_value,
                            const DictionaryValue& value,
                            const std::string& key);

void ExpectDictDictionaryValue(const DictionaryValue& expected_value,
                               const DictionaryValue& value,
                               const std::string& key);

void ExpectDictIntegerValue(int expected_value,
                            const DictionaryValue& value,
                            const std::string& key);

void ExpectDictListValue(const ListValue& expected_value,
                         const DictionaryValue& value,
                         const std::string& key);

void ExpectDictStringValue(const std::string& expected_value,
                           const DictionaryValue& value,
                           const std::string& key);

// Takes ownership of |actual|.
void ExpectStringValue(const std::string& expected_str,
                       StringValue* actual);

}  // namespace base

#endif  // BASE_TEST_VALUES_TEST_UTIL_H_
