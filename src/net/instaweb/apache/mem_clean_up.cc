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
//
// Author: jmarantz@google.com (Joshua Marantz)

#include "googleurl/src/url_util.h"
#include "net/instaweb/htmlparse/public/html_escape.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "third_party/protobuf2/src/src/google/protobuf/stubs/common.h"

// Clean up valgrind-based memory-leak checks by deleting statically allocated
// data from various libraries.  This must be called both from unit-tests
// and from the Apache module, so that valgrind can be run on both of them.

namespace {

class MemCleanUp {
 public:
  ~MemCleanUp() {
    net_instaweb::CssFilter::Terminate();
    net_instaweb::HtmlEscape::ShutDown();
    google::protobuf::ShutdownProtobufLibrary();
    url_util::Shutdown();
  }
};
MemCleanUp mem_clean_up;

}  // namespace
