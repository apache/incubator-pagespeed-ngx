// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Chromium's process.cc pulls a lot of file related functions into the
// base package. We don't need them, so strip down all the code.

#include "base/logging.h"
#include "base/process/process.h"

namespace base {

// Returns the id of the current process.
ProcessId GetCurrentProcId() {
  DCHECK(false);  // we don't actually expect this to be called.
  return 0;
}

}  // namespace base
