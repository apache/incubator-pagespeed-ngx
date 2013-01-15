// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NULLABLE_STRING16_H_
#define BASE_NULLABLE_STRING16_H_
#pragma once

#include "base/string16.h"

// This class is a simple wrapper for string16 which also contains a null
// state.  This should be used only where the difference between null and
// empty is meaningful.
class NullableString16 {
 public:
  NullableString16() : is_null_(false) { }
  explicit NullableString16(bool is_null) : is_null_(is_null) { }
  NullableString16(const string16& string, bool is_null)
      : string_(string), is_null_(is_null) {
  }

  const string16& string() const { return string_; }
  bool is_null() const { return is_null_; }

 private:
  string16 string_;
  bool is_null_;
};

#endif  // BASE_NULLABLE_STRING16_H_
