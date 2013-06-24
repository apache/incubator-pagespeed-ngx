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

#include "net/instaweb/rewriter/public/downstream_caching_directives.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

TEST(DownstreamCachingDirectivesTest, SupportsImageInlining) {
  DownstreamCachingDirectives directives;
  EXPECT_TRUE(directives.SupportsImageInlining());
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningEmptyRequestHeaders) {
  DownstreamCachingDirectives directives;
  RequestHeaders request_headers;
  request_headers.Add(kPsaCapabilityList, "");
  directives.ParseCapabilityListFromRequestHeaders(request_headers);
  EXPECT_FALSE(directives.SupportsImageInlining());
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningViaRequestHeaders) {
  DownstreamCachingDirectives directives;
  RequestHeaders request_headers;
  request_headers.Add(kPsaCapabilityList,
                      RewriteOptions::FilterId(RewriteOptions::kInlineImages));
  directives.ParseCapabilityListFromRequestHeaders(request_headers);
  EXPECT_TRUE(directives.SupportsImageInlining());
}

}  // namespace net_instaweb
