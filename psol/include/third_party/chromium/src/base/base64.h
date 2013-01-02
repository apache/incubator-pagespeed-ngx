// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE64_H__
#define BASE_BASE64_H__
#pragma once

#include <string>

#include "base/base_api.h"

namespace base {

// Encodes the input string in base64.  Returns true if successful and false
// otherwise.  The output string is only modified if successful.
BASE_API bool Base64Encode(const std::string& input, std::string* output);

// Decodes the base64 input string.  Returns true if successful and false
// otherwise.  The output string is only modified if successful.
BASE_API bool Base64Decode(const std::string& input, std::string* output);

}  // namespace base

#endif  // BASE_BASE64_H__
