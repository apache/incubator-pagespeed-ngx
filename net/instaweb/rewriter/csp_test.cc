/*
 * Copyright 2017 Google Inc.
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

#include "net/instaweb/rewriter/public/csp.h"

#include <memory>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

TEST(CspParseSourceTest, Quoted) {
  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kSelf),
      CspSourceExpression::Parse("'self' "));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kSelf),
      CspSourceExpression::Parse("   'sElf' "));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kStrictDynamic),
      CspSourceExpression::Parse("  \t 'strict-dynamic' "));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kUnsafeInline),
      CspSourceExpression::Parse("'unsafe-inline'"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kUnsafeEval),
      CspSourceExpression::Parse("'unsafe-eval'"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kUnsafeHashedAttributes),
      CspSourceExpression::Parse("'unsafe-hashed-attribUtes'"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kUnknown),
      CspSourceExpression::Parse("'nonce-qwertyu12345'"));
}

TEST(CspParseSourceTest, NonQuoted) {
  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kUnknown),
      CspSourceExpression::Parse("   "));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kSchemeSource, "https:"),
      CspSourceExpression::Parse(" https:"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kSchemeSource,
                          "weird-schema+-1.0:"),
      CspSourceExpression::Parse("weird-schema+-1.0:"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kHostSource, "*.example.com"),
      CspSourceExpression::Parse("*.example.com"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kHostSource,
                          "http://www.example.com/dir"),
      CspSourceExpression::Parse("http://www.example.com/dir"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kHostSource,
                          "http://www.example.com/dir/file.js"),
      CspSourceExpression::Parse("http://www.example.com/dir/file.js"));

  EXPECT_EQ(
      CspSourceExpression(CspSourceExpression::kHostSource, "*"),
      CspSourceExpression::Parse("*"));
}

TEST(CspParseTest, Empty) {
  std::unique_ptr<CspPolicy> policy(CspPolicy::Parse("   "));
  EXPECT_EQ(policy, nullptr);
}

TEST(CspParseTest, Basic) {
  std::unique_ptr<CspPolicy> policy(CspPolicy::Parse(
    "default-src *; script-src 'unsafe-inline' 'unsafe-eval'"));
  ASSERT_TRUE(policy != nullptr);
  ASSERT_TRUE(policy->SourceListFor(CspDirective::kDefaultSrc) != nullptr);
  const std::vector<CspSourceExpression>& default_src =
      policy->SourceListFor(CspDirective::kDefaultSrc)->expressions();
  ASSERT_EQ(1, default_src.size());
  EXPECT_EQ(CspSourceExpression::kHostSource, default_src[0].kind());
  EXPECT_EQ("*", default_src[0].param());

  ASSERT_TRUE(policy->SourceListFor(CspDirective::kScriptSrc) != nullptr);
  const std::vector<CspSourceExpression>& script_src =
      policy->SourceListFor(CspDirective::kScriptSrc)->expressions();
  ASSERT_EQ(2, script_src.size());
  EXPECT_EQ(CspSourceExpression::kUnsafeInline, script_src[0].kind());
  EXPECT_EQ(CspSourceExpression::kUnsafeEval, script_src[1].kind());
}

TEST(CspParseTest, Repeated) {
  // Repeating within same policy doesn't do anything.
  std::unique_ptr<CspPolicy> policy(CspPolicy::Parse(
    "script-src 'unsafe-inline' 'unsafe-eval'; script-src 'strict-dynamic'"));
  ASSERT_TRUE(policy != nullptr);
  ASSERT_TRUE(policy->SourceListFor(CspDirective::kScriptSrc) != nullptr);
  const std::vector<CspSourceExpression>& script_src =
      policy->SourceListFor(CspDirective::kScriptSrc)->expressions();
  ASSERT_EQ(2, script_src.size());
  EXPECT_EQ(CspSourceExpression::kUnsafeInline, script_src[0].kind());
  EXPECT_EQ(CspSourceExpression::kUnsafeEval, script_src[1].kind());
}

}  // namespace

}  // namespace net_instaweb
