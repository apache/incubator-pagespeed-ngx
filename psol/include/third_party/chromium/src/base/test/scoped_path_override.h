// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_PATH_OVERRIDE_H_
#define BASE_TEST_SCOPED_PATH_OVERRIDE_H_

#include "base/basictypes.h"
#include "base/scoped_temp_dir.h"

class FilePath;

namespace base {

// Sets a path override on construction, and removes it when the object goes out
// of scope. This class is intended to be used by tests that need to override
// paths to ensure their overrides are properly handled and reverted when the
// scope of the test is left.
class ScopedPathOverride {
 public:
  // Contructor that intializes the override to a scoped temp directory.
  explicit ScopedPathOverride(int key);
  // Constructor that would use a path provided by the user.
  ScopedPathOverride(int key, const FilePath& dir);
  ~ScopedPathOverride();

 private:
  int key_;
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPathOverride);
};

}  // namespace base

#endif  // BASE_TEST_SCOPED_PATH_OVERRIDE_H_
