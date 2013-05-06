/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)
//
// This contains classes that abstract away creation of threads and
// synchronization primitives.
// - ThreadSystem (base class): acts as a factory for mutexes compatible
//   with some runtime environment and must be passed to Thread ctor to use its
//   threading abilities.
// - ThreadImpl: abstract interface used to communicate with threading
//   backends by Thread instances.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_

// TODO(huibao): Remove this forwarding header and update references.
#include "pagespeed/kernel/base/thread_system.h"

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_
