// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"

namespace base {
namespace debug {

// Sleep until HighResNow has advanced by at least |elapsed|.
void HighResSleepForTraceTest(base::TimeDelta elapsed);

}  // namespace debug
}  // namespace base
