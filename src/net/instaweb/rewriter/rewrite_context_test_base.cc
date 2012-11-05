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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the RewriteContext class.  This is made simplest by
// setting up some dummy rewriters in our test framework.

#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

const char TrimWhitespaceRewriter::kFilterId[] = "tw";
const char TrimWhitespaceSyncFilter::kFilterId[] = "ts";
const char UpperCaseRewriter::kFilterId[] = "uc";
const char NestedFilter::kFilterId[] = "nf";
const char CombiningFilter::kFilterId[] = "cr";

// TODO(jmarantz): move method implementations from rewrite_context_test_base.h
// to here.

TrimWhitespaceRewriter::~TrimWhitespaceRewriter() {
}

UpperCaseRewriter::~UpperCaseRewriter() {
}

NestedFilter::~NestedFilter() {
}

NestedFilter::Context::~Context() {
  STLDeleteElements(&strings_);
}

CombiningFilter::~CombiningFilter() {
}

RewriteContextTest::~RewriteContextTest() {
}

}  // namespace net_instaweb
