// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_BIND_OBJC_BLOCK_H_
#define BASE_MAC_BIND_OBJC_BLOCK_H_

#include "base/base_export.h"
#include "base/callback_forward.h"

namespace base {

// Construct a closure from an objective-C block.
BASE_EXPORT base::Closure BindBlock(void(^block)());

}  // namespace base

#endif  // BASE_MAC_BIND_OBJC_BLOCK_H_
