// Copyright 2011 Google Inc.
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

#include "net/instaweb/rewriter/public/process_context.h"

#include <cstddef>

#include "base/logging.h"
#include "googleurl/src/url_util.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/util/public/gflags.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

#include "third_party/protobuf/src/google/protobuf/stubs/common.h"
using namespace google;  // NOLINT

// Clean up valgrind-based memory-leak checks by deleting statically allocated
// data from various libraries.  This must be called both from unit-tests
// and from the Apache module, so that valgrind can be run on both of them.

namespace {

pagespeed::js::JsTokenizerPatterns* tokenizer_patterns = NULL;

}  // namespace

namespace net_instaweb {

ProcessContext::ProcessContext() {
  HtmlKeywords::Init();
  if (tokenizer_patterns != NULL) {
    LOG(DFATAL) << "tokenizer_patterns == NULL.  Double construct "
        "of ProcessContext?";
  } else {
    tokenizer_patterns = new pagespeed::js::JsTokenizerPatterns;
  }

  // googleurl/src/url_util.cc lazily initializes its
  // "standard_schemes" table in a thread-unsafe way and so it must be
  // explicitly initialized prior to thread creation, and explicitly
  // terminated after thread quiescence.
  url_util::Initialize();
}

ProcessContext::~ProcessContext() {
  // Clean up statics from third_party code first.

  // The command-line flags structures are lazily initialized, but
  // they are done so in static constructors resulting from DEFINE_int32
  // and other similar macros.  So they must happen prior to threads
  // starting up.
  ShutDownCommandLineFlags();

  // The protobuf shutdown infrastructure is lazily initialized in a threadsafe
  // manner.  See third_party/protobuf/src/google/protobuf/stubs/common.cc,
  // function InitShutdownFunctionsOnce.
  google::protobuf::ShutdownProtobufLibrary();

  url_util::Shutdown();
  HtmlKeywords::ShutDown();
  if (tokenizer_patterns == NULL) {
    LOG(DFATAL) << "tokenizer_patterns == NULL.  Double destruct "
        "of ProcessContext?";
  } else {
    delete tokenizer_patterns;
    tokenizer_patterns = NULL;
  }
}

const pagespeed::js::JsTokenizerPatterns*
ProcessContext::js_tokenizer_patterns() {
  // Lazy-initialize tokenizer_patterns if ProcessContext was not
  // instantiated, to support binaries that don't pre-instantiate
  // ProcessContext.  Note that the patterns memory will not be freed
  // in that case.
  if (tokenizer_patterns == NULL) {
    tokenizer_patterns = new pagespeed::js::JsTokenizerPatterns;
  }
  return tokenizer_patterns;
}

}  // namespace net_instaweb
