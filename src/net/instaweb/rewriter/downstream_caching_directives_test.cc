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
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

TEST(DownstreamCachingDirectivesTest, SupportsImageInlining) {
  DownstreamCachingDirectives directives;
  EXPECT_TRUE(directives.SupportsImageInlining());
}

void VerifySupportForCapability(const StringPiece& header_value,
                                bool expected_support) {
  DownstreamCachingDirectives directives;
  RequestHeaders request_headers;
  request_headers.Add(kPsaCapabilityList, header_value);
  directives.ParseCapabilityListFromRequestHeaders(request_headers);
  EXPECT_TRUE(directives.SupportsImageInlining() == expected_support) <<
      "SupportsImageInlining should have been " << expected_support <<
      " for header value " << header_value;
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningWithNoConstraints) {
  VerifySupportForCapability("NoCapabilitiesSpecified", true);
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningEmptyRequestHeaders) {
  VerifySupportForCapability("", false);
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningViaRequestHeaders) {
  StringPiece capability =
      RewriteOptions::FilterId(RewriteOptions::kInlineImages);
  // "ii" should mean supported.
  VerifySupportForCapability(capability, true);
  // "iix" should mean unsupported.
  VerifySupportForCapability(StrCat(capability, "x"), false);
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningViaRequestHeadersWithColonEnding) {
  StringPiece capability =
      RewriteOptions::FilterId(RewriteOptions::kInlineImages);
  // "ii:" should mean supported.
  VerifySupportForCapability(StrCat(capability, ":"), true);
  // "ii:abc" should mean supported.
  VerifySupportForCapability(StrCat(capability, ":abc"), true);
  // "xii:" should mean unsupported.
  VerifySupportForCapability(StrCat("x", capability, ":"), false);
  // "iix:" should mean unsupported.
  VerifySupportForCapability(StrCat(capability, "x:"), false);
  // ",ii:" should mean supported.
  VerifySupportForCapability(StrCat(",", capability, ":"), true);
  // ",iix:" should mean unsupported.
  VerifySupportForCapability(StrCat(",", capability, "x:"), false);
  // "abc,ii:" should mean supported.
  VerifySupportForCapability(StrCat("abc,", capability, ":"), true);
  // "abc,ii:def" should mean supported.
  VerifySupportForCapability(StrCat("abc,", capability, ":def"), true);
}

TEST(DownstreamCachingDirectivesTest,
       SupportsImageInliningViaRequestHeadersWithComma) {
  StringPiece capability =
      RewriteOptions::FilterId(RewriteOptions::kInlineImages);
  // "ii," should mean supported.
  VerifySupportForCapability(StrCat(capability, ","), true);
  // "ii,abc" should mean supported.
  VerifySupportForCapability(StrCat(capability, ",abc"), true);
  // "xii," should mean unsupported.
  VerifySupportForCapability(StrCat("x", capability, ","), false);
  // "iix," should mean unsupported.
  VerifySupportForCapability(StrCat(capability, "x,"), false);
  // ",iix" should mean unsupported.
  VerifySupportForCapability(StrCat(",", capability, "x"), false);
  // ",ii," should mean supported.
  VerifySupportForCapability(StrCat(",", capability, ","), true);
  // "abc,ii," should mean supported.
  VerifySupportForCapability(StrCat("abc,", capability, ","), true);
  // "abc,ii,def" should mean supported.
  VerifySupportForCapability(StrCat("abc,", capability, ",def"), true);
}

}  // namespace net_instaweb
