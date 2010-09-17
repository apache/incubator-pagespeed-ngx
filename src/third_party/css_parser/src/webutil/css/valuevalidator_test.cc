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

// Copyright 2008 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yi-An Huang)

#include "webutil/css/valuevalidator.h"

#include <string>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "webutil/css/string.h"

namespace Css {

class ValueValidatorTest : public testing::Test {
 protected:
  const ValueValidator& validator() {
    return *ValueValidator::Get();
  }
};

TEST_F(ValueValidatorTest, types) {
  EXPECT_TRUE(validator().IsValidType(Property::COLOR, Value::IDENT));
  EXPECT_TRUE(validator().IsValidType(Property::COLOR, Value::DEFAULT));
  EXPECT_TRUE(validator().IsValidType(Property::COLOR, Value::UNKNOWN));
  EXPECT_TRUE(validator().IsValidType(Property::COLOR, Value::COLOR));
  EXPECT_FALSE(validator().IsValidType(Property::COLOR, Value::STRING));
  EXPECT_FALSE(validator().IsValidType(Property::COLOR, Value::URI));
  EXPECT_FALSE(validator().IsValidType(Property::COLOR, Value::FUNCTION));
}

TEST_F(ValueValidatorTest, identifiers) {
  EXPECT_TRUE(validator().IsValidIdentifier(Property::COLOR,
                                            Identifier::INHERIT));
  EXPECT_FALSE(validator().IsValidIdentifier(Property::COLOR,
                                             Identifier::OTHER));
  // font-family take all identifiers.
  EXPECT_TRUE(validator().IsValidIdentifier(Property::FONT_FAMILY,
                                            Identifier::SERIF));
  EXPECT_TRUE(validator().IsValidIdentifier(Property::FONT_FAMILY,
                                            Identifier::OTHER));
  // FIXME(yian): Is this right?
  EXPECT_FALSE(validator().IsValidIdentifier(Property::FONT_FAMILY,
                                             Identifier::NORMAL));
}

TEST_F(ValueValidatorTest, numbers) {
  // misc units
  EXPECT_FALSE(validator().IsValidNumber(
      Property::HEIGHT, Value(0, UTF8ToUnicodeText(string("unit"), false)),
      false));
  EXPECT_FALSE(validator().IsValidNumber(
      Property::HEIGHT, Value(0, Value::RAD), false));
  // percent
  EXPECT_TRUE(validator().IsValidNumber(
      Property::HEIGHT, Value(0, Value::PERCENT), false));
  EXPECT_FALSE(validator().IsValidNumber(
      Property::Z_INDEX, Value(0, Value::PERCENT), false));
  // no-unit
  EXPECT_FALSE(validator().IsValidNumber(
      Property::HEIGHT, Value(1, Value::NO_UNIT), false));
  EXPECT_TRUE(validator().IsValidNumber(
      Property::HEIGHT, Value(0, Value::NO_UNIT), false));
  EXPECT_TRUE(validator().IsValidNumber(
      Property::HEIGHT, Value(1, Value::NO_UNIT), true));
  EXPECT_TRUE(validator().IsValidNumber(
      Property::Z_INDEX, Value(1, Value::NO_UNIT), false));
  // lengths
  EXPECT_TRUE(validator().IsValidNumber(
      Property::HEIGHT, Value(1, Value::PX), false));
  EXPECT_FALSE(validator().IsValidNumber(
      Property::Z_INDEX, Value(1, Value::PX), false));
  // negative
  EXPECT_TRUE(validator().IsValidNumber(
      Property::BOTTOM, Value(-1, Value::PX), false));
  EXPECT_FALSE(validator().IsValidNumber(
      Property::HEIGHT, Value(-1, Value::PX), false));
}

TEST_F(ValueValidatorTest, combined) {
  Identifier transparent_identifier(Identifier::TRANSPARENT);
  Value transparent(transparent_identifier);
  Value some_string(Value::STRING, UTF8ToUnicodeText(string("string"), false));
  EXPECT_TRUE(validator().IsValidValue(Property::BACKGROUND_COLOR,
                                       transparent, false));
  EXPECT_FALSE(validator().IsValidValue(Property::COLOR, transparent, false));
  EXPECT_FALSE(validator().IsValidValue(Property::COLOR, some_string, false));
}

}  // namespace
