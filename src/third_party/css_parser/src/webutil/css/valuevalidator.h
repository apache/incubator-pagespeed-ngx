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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yi-An Huang)

#ifndef WEBUTIL_CSS_VALUEVALIDATOR_H__
#define WEBUTIL_CSS_VALUEVALIDATOR_H__

#include <vector>

#include "testing/production_stub/public/gunit_prod.h"
#include "util/gtl/singleton.h"
#include "webutil/css/property.h"
#include "webutil/css/value.h"
#include "webutil/css/identifier.h"

namespace Css {

struct PropertyValidationInfo;

class ValueValidator {
 public:
  static ValueValidator* Get() { return Singleton<ValueValidator>::get(); }

  // Is value is a valid value for property prop?
  bool IsValidValue(Property::Prop prop, const Value& value,
                    bool quirks_mode) const;

  // TODO(sligocki): Chromium's Singleton<> is not playing well with this class
  // and so we've had to make the constructor/destructor public. Look into this.
  // Default Constructor; only used for the Singleton, default instance
  ValueValidator();
  ~ValueValidator();

private:
  // Is type is a valid type for property prop?
  bool IsValidType(Property::Prop prop, Value::ValueType type) const;

  // Is ident is a valid identifier for property prop?
  bool IsValidIdentifier(Property::Prop prop, Identifier::Ident ident) const;

  // Is number valid for property prop?
  bool IsValidNumber(Property::Prop prop, const Value& value,
                     bool quirks_mode) const;

  std::vector<PropertyValidationInfo*> validation_info_;
  friend class Singleton<ValueValidator>;
  FRIEND_TEST(ValueValidatorTest, types);
  FRIEND_TEST(ValueValidatorTest, identifiers);
  FRIEND_TEST(ValueValidatorTest, numbers);

  DISALLOW_COPY_AND_ASSIGN(ValueValidator);
};

}  // namespace

#endif  // WEBUTIL_CSS_VALUEVALIDATOR_H__
