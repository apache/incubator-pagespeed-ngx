// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VERSION_H_
#define BASE_VERSION_H_
#pragma once

#include <string>
#include <vector>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"

// Version represents a dotted version number, like "1.2.3.4", supporting
// parsing and comparison.
class BASE_API Version {
 public:
  // The only thing you can legally do to a default constructed
  // Version object is assign to it.
  Version();

  ~Version();

  // Initializes from a decimal dotted version number, like "0.1.1".
  // Each component is limited to a uint16. Call IsValid() to learn
  // the outcome.
  explicit Version(const std::string& version_str);

  // Returns true if the object contains a valid version number.
  bool IsValid() const;

  // Returns NULL if the string is not in the proper format.
  // Caller is responsible for freeing the Version object once done.
  // DO NOT USE FOR NEWER CODE.
  static Version* GetVersionFromString(const std::string& version_str);

  // Creates a copy of this version object. Caller takes ownership.
  // DO NOT USE FOR NEWER CODE.
  Version* Clone() const;

  bool Equals(const Version& other) const;

  // Returns -1, 0, 1 for <, ==, >.
  int CompareTo(const Version& other) const;

  // Return the string representation of this version.
  const std::string GetString() const;

  const std::vector<uint16>& components() const { return components_; }

 private:
  std::vector<uint16> components_;
};

#endif  // BASE_VERSION_H_
