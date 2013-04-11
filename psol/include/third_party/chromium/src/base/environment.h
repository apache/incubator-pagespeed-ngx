// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ENVIRONMENT_H_
#define BASE_ENVIRONMENT_H_

#include <string>

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {

namespace env_vars {

#if defined(OS_POSIX)
BASE_EXPORT extern const char kHome[];
#endif

}  // namespace env_vars

class BASE_EXPORT Environment {
 public:
  virtual ~Environment();

  // Static factory method that returns the implementation that provide the
  // appropriate platform-specific instance.
  static Environment* Create();

  // Gets an environment variable's value and stores it in |result|.
  // Returns false if the key is unset.
  virtual bool GetVar(const char* variable_name, std::string* result) = 0;

  // Syntactic sugar for GetVar(variable_name, NULL);
  virtual bool HasVar(const char* variable_name);

  // Returns true on success, otherwise returns false.
  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) = 0;

  // Returns true on success, otherwise returns false.
  virtual bool UnSetVar(const char* variable_name) = 0;
};

}  // namespace base

#endif  // BASE_ENVIRONMENT_H_
