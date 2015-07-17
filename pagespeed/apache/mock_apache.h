// Copyright 2015 Google Inc.
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
//
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_APACHE_MOCK_APACHE_H_
#define PAGESPEED_APACHE_MOCK_APACHE_H_

#include "pagespeed/kernel/base/string_util.h"

struct request_rec;

namespace net_instaweb {

namespace MockApache {

// When unit testing code that manipulates and Apache request_rec by calling
// apache functions like ap_rwrite or ap_rflush we don't want to actually run
// Apache.  Instead, we link in mock implementations of these functions that
// actually just write to a global variable indicating that they were called.
//
// If you link mock_apache.cc to supply any of these function mocks you must
// call Initialize() before any ap_* calls and Terminate() after them.  To
// verify that higher level calls led to the correct lower level actions, call
// ActionsSinceLastCall() to get a text representation of past actions.
//
// Most of these calls need a properly initialized request_rec.  Use
// PrepareRequest/CleanupRequest for that.
//
// Example:
//
//    MockApache::Initialize();
//    request_rec r;
//    MockApache::PrepareRequest(&r);
//    SomethingThatCallsApRWrite("foo", &r)
//    SomethingThatCallsApRFlush(&r)
//    EXPECT_EQ("ap_rwrite(foo) ap_rflush()",
//              MockApache::ActionsSinceLastCall());
//    MockApache::CleanupRequest(&r);
//    MockApache::Terminate();

// Call once before any uses of MockApache.
void Initialize();
// Call once after any uses of MockApache.
void Terminate();

// Call on every request to create a pool for it and allocate initial
// structures.
void PrepareRequest(request_rec* request);
// Call on every request when you're done with it to clean up its pool.
void CleanupRequest(request_rec* request);

// Call to verify that the correct underlying apache calls were made.  Returns a
// space separated string of the calls along with serialized arguments when
// appropriate.
GoogleString ActionsSinceLastCall();

}  // namespace MockApache

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_MOCK_APACHE_H_

