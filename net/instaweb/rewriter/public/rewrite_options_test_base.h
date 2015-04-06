/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_TEST_BASE_H_

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

// Helper class to for tests that need to initialized RewriteOptions.  This
// class is templated so any flavor of RewriteOptions can be used.
template<class OptionsClass>
class RewriteOptionsTestBase : public testing::Test {
 public:
  RewriteOptionsTestBase()
      : thread_system_(Platform::CreateThreadSystem()) {
    OptionsClass::Initialize();
  }

  ~RewriteOptionsTestBase() {
    OptionsClass::Terminate();
  }

  ThreadSystem* thread_system() { return thread_system_.get(); }
  OptionsClass* NewOptions() { return new OptionsClass(thread_system()); }

  scoped_ptr<ThreadSystem> thread_system_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_TEST_BASE_H_
