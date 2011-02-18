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

// Copyright 2006 Google Inc. All Rights Reserved.
// Author: dpeng@google.com (Daniel Peng)

#include "util/gtl/stl_util-inl.h"

#include "webutil/css/value.h"

#include "base/logging.h"
#include "strings/memutil.h"
#include "webutil/html/htmlcolor.h"

namespace Css {

//
// Static constants.
//

const char* const Value::kDimensionUnitText[] = {
  "em", "ex", "px", "cm", "mm", "in", "pt", "pc",
  "deg", "rad", "grad", "ms", "s", "hz", "khz", "%", "OTHER", "" };

//
// Constructors.
//

Value::Value(ValueType ty)
    : type_(ty),
      color_(0, 0, 0) {
  DCHECK(ty == DEFAULT || ty == UNKNOWN);
}

Value::Value(float num, const UnicodeText& unit)
    : type_(NUMBER),
      num_(num),
      color_(0, 0, 0) {
  unit_ = UnitFromText(unit.utf8_data(), unit.utf8_length());
  if (unit_ == OTHER)
    str_ = unit;
}

Value::Value(float num, Unit unit)
    : type_(NUMBER),
      num_(num),
      unit_(unit),
      color_(0, 0, 0)  {
  DCHECK_NE(unit, OTHER);
}

Value::Value(ValueType ty, const UnicodeText& str)
    : type_(ty),
      str_(str),
      color_(0, 0, 0) {
  DCHECK(ty == STRING || ty == URI);
}

Value::Value(const Identifier& identifier)
    : type_(IDENT),
      identifier_(identifier),
      color_(0, 0, 0) {
}

Value::Value(const Identifier::Ident ident)
    : type_(IDENT),
      identifier_(Identifier(ident)),
      color_(0, 0, 0) {
}

Value::Value(ValueType ty, FunctionParameters* params)
    : type_(ty),
      params_(params),
      color_(0, 0, 0) {
  DCHECK(params != NULL);
  DCHECK(ty == RECT);
}

Value::Value(const UnicodeText& func, FunctionParameters* params)
    : type_(FUNCTION),
      str_(func),
      params_(params),
      color_(0, 0, 0) {
  DCHECK(params != NULL);
}

Value::Value(HtmlColor c)
    : type_(COLOR),
      color_(c) {
}

Value::Value(const Value& other)
  : type_(other.type_),
    num_(other.num_),
    unit_(other.unit_),
    identifier_(other.identifier_),
    str_(other.str_),
    params_(new FunctionParameters),
    color_(other.color_) {
  if (other.params_.get() != NULL) {
    params_->Copy(*other.params_);
  }
}

Value& Value::operator=(const Value& other) {
  if (this == &other) return *this;
  type_ = other.type_;
  num_ = other.num_;
  unit_ = other.unit_;
  identifier_ = other.identifier_;
  str_ = other.str_;
  color_ = other.color_;
  if (other.params_.get() != NULL) {
    params_->Copy(*other.params_);
  } else {
    params_.reset();
  }
  return *this;
}

bool Value::Equals(const Value& other) const {
  if (type_ != other.type_) return false;
  switch (type_) {
    case DEFAULT:
    case UNKNOWN:
      return true;
    case NUMBER:
      return unit_ == other.unit_ && num_ == other.num_;
    case URI:
    case STRING:
      return str_ == other.str_;
    case IDENT:
      if (identifier_.ident() != other.identifier_.ident())
        return false;
      if (identifier_.ident() == Identifier::OTHER)
        return identifier_.ident_text() == other.identifier_.ident_text();
      return true;
    case COLOR:
      if (color_.IsDefined() != other.color_.IsDefined())
        return false;
      if (color_.IsDefined())
        return color_.rgb() == other.color_.rgb();
      return true;
    case FUNCTION:
      if (str_ != other.str_)
        return false;
      // pass through
    case RECT:
      if (params_.get() == NULL)
        return other.params_.get() == NULL;
      return params_->Equals(*other.params_);
    default:
      LOG(FATAL) << "Unknown type:" << type_;
  }
}

//
// Static functions mapping between units and strings
//

Value::Unit Value::UnitFromText(const char* str, int len) {
  switch (len) {
    case 0:
      return NO_UNIT;
    case 1:
      switch (str[0]) {
        case '%': return PERCENT;
        case 's': case 'S': return S;
        default: return OTHER;
      }
    case 2:
      switch (str[0]) {
        case 'e': case 'E':
          switch (str[1]) {
            case 'm': case 'M': return EM;
            case 'x': case 'X': return EX;
            default: return OTHER;
          }
        case 'p': case 'P':
          switch (str[1]) {
            case 'x': case 'X':  return PX;
            case 't': case 'T': return PT;
            case 'c': case 'C': return PC;
            default: return OTHER;
          }
        case 'c': case 'C':
          switch (str[1]) {
            case 'm': case 'M': return CM;
            default: return OTHER;
          }
        case 'm': case 'M':
          switch (str[1]) {
            case 'm': case 'M': return MM;
            case 's': case 'S': return MS;
            default: return OTHER;
          }
        case 'i': case 'I':
          switch (str[1]) {
            case 'n': case 'N': return IN;
            default: return OTHER;
          }
        case 'h': case 'H':
          switch (str[1]) {
            case 'z': case 'Z': return HZ;
            default: return OTHER;
          }
        default:
          return OTHER;
      }
    case 3:
      if (memcasecmp(str, "deg", 3) == 0) return DEG;
      if (memcasecmp(str, "rad", 3) == 0) return RAD;
      if (memcasecmp(str, "khz", 3) == 0) return KHZ;
      return OTHER;
    case 4:
      if (memcasecmp(str, "grad", 3) == 0) return GRAD;
      return OTHER;
    default:
      return OTHER;
  }
}

const char* Value::TextFromUnit(Unit u) {
  DCHECK_LT(u, NUM_UNITS);
  COMPILE_ASSERT(arraysize(kDimensionUnitText) == NUM_UNITS, dimensionunitsize);

  return kDimensionUnitText[u];
}

//
// Accessors.
//

Value::ValueType Value::GetLexicalUnitType() const {
  return type_;
}

string Value::GetDimensionUnitText() const {
  DCHECK_EQ(type_, NUMBER);
  if (unit_ == OTHER)
    return string(str_.utf8_data(), str_.utf8_length());
  else
    return TextFromUnit(unit_);
}

Value::Unit Value::GetDimension() const {
  DCHECK_EQ(type_, NUMBER);
  return unit_;
}

int Value::GetIntegerValue() const {
  DCHECK_EQ(type_, NUMBER);
  return static_cast<int>(num_);
}

float Value::GetFloatValue() const {
  DCHECK_EQ(type_, NUMBER);
  return num_;
}

const Values* Value::GetParameters() const {
  DCHECK(type_ == FUNCTION || type_ == RECT);
  return params_->values();
}

const FunctionParameters* Value::GetParametersWithSeparators() const {
  DCHECK(type_ == FUNCTION || type_ == RECT);
  return params_.get();
}

const UnicodeText& Value::GetFunctionName() const {
  DCHECK_EQ(type_, FUNCTION);
  return str_;
}

const UnicodeText& Value::GetStringValue() const {
  DCHECK(type_ == URI || type_ == STRING);
  return str_;
}

UnicodeText Value::GetIdentifierText() const {
  DCHECK_EQ(type_, IDENT);
  return identifier_.ident_text();
}

const Identifier& Value::GetIdentifier() const {
  DCHECK_EQ(type_, IDENT);
  return identifier_;
}

const HtmlColor& Value::GetColorValue() const {
  DCHECK_EQ(type_, COLOR);
  return color_;
}

Values::~Values() { STLDeleteElements(this); }

FunctionParameters::~FunctionParameters() {}

void FunctionParameters::AddSepValue(Separator separator, Value* value) {
  separators_.push_back(separator);
  values_->push_back(value);
  DCHECK_EQ(separators_.size(), values_->size());
}

bool FunctionParameters::Equals(const FunctionParameters& other) const {
  if (size() != other.size()) {
    return false;
  }
  for (int i = 0, n = size(); i < n; ++i) {
    if (!value(i)->Equals(*other.value(i)) ||
        separator(i) != other.separator(i)) {
      return false;
    }
  }
  return true;
}

void FunctionParameters::Copy(const FunctionParameters& other) {
  if (this != &other) {
    int size = other.size();
    values_->clear();
    values_->reserve(size);
    separators_.clear();
    separators_.reserve(size);
    for (int i = 0; i < size; ++i) {
      values_->push_back(new Value(*other.values_->at(i)));
      separators_.push_back(other.separators_[i]);
    }
  }
  DCHECK(this->Equals(other));
}

}  // namespace
