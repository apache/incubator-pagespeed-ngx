// Copyright 2010 Google Inc.
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

#ifndef NACL_HEADER_STUBS_MEMORY_H_
#define NACL_HEADER_STUBS_MEMORY_H_

#ifndef __native_client__
#error This file should only be used when compiling for Native Client.
#endif

// Some of our third-party files include <memory.h> in order to get things like
// memcpy and memset, but NaCl doesn't have <memory.h>.  Including <string.h>
// instead is good enough for our purposes.
#include <string.h>

#endif  // NACL_HEADER_STUBS_MEMORY_H_
