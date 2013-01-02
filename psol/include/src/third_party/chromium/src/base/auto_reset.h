// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_AUTO_RESET_H_
#define BASE_AUTO_RESET_H_
#pragma once

#include "base/basictypes.h"

// AutoResetValue is useful for setting a variable to some value only
// during a particular scope.  If you have code that has to add "var =
// false;" or "var = old_var;" at all the exit points of a block, for
// example, you would benefit from using this instead.
//
// This should be obvious, but note that the AutoResetValue instance
// should have a shorter lifetime than the scoped_variable, to prevent
// writing to invalid memory when the AutoResetValue goes out of
// scope.

template<typename T>
class AutoReset {
 public:
  AutoReset(T* scoped_variable, T new_value)
      : scoped_variable_(scoped_variable),
        original_value_(*scoped_variable) {
    *scoped_variable_ = new_value;
  }

  ~AutoReset() { *scoped_variable_ = original_value_; }

 private:
  T* scoped_variable_;
  T original_value_;

  DISALLOW_COPY_AND_ASSIGN(AutoReset);
};

#endif  // BASE_AUTO_RESET_H_
