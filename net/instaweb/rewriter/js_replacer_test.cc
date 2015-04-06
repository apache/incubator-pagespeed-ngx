/*
 * Copyright 2014 Google Inc.
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

#include "net/instaweb/rewriter/public/js_replacer.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

using pagespeed::js::JsTokenizerPatterns;

namespace net_instaweb {

namespace {

class JsReplacerTest : public testing::Test {
 public:
  JsReplacerTest() : replacer_(&patterns_) {}

  void AppendTail(GoogleString* str) {
    StrAppend(str, " with tail");
  }

  void AppendHead(GoogleString* str) {
    *str = "head with " + *str;
  }

 protected:
  JsTokenizerPatterns patterns_;
  JsReplacer replacer_;
};

TEST_F(JsReplacerTest, EmptyNoOp) {
  const char kIn[] = "function foo() {\n  return 42;\n}";
  GoogleString out;
  replacer_.Transform(kIn, &out);
  EXPECT_EQ(kIn, out);
}

TEST_F(JsReplacerTest, BasicMatch) {
  const char kIn[] = "a.b.c = \"42\"; document.domain = 'whatever.com';";
  const char kOut[] =
      "a.b.c = \"42\"; document.domain = 'whatever.com with tail';";
  scoped_ptr<JsReplacer::StringRewriter> rewriter(
      NewPermanentCallback(this, &JsReplacerTest::AppendTail));
  replacer_.AddPattern("document", "domain", rewriter.get());

  GoogleString out;
  replacer_.Transform(kIn, &out);
  EXPECT_EQ(kOut, out);
}

TEST_F(JsReplacerTest, RedundantPattern) {
  // Make sure the documented behavior of redundant patterns actually happens.
  const char kIn[] = "a.b.c = \"42\"; document.domain = 'whatever.com';";
  const char kOut[] =
      "a.b.c = \"42\"; document.domain = 'whatever.com with tail';";
  scoped_ptr<JsReplacer::StringRewriter> rewriter(
      NewPermanentCallback(this, &JsReplacerTest::AppendTail));
  scoped_ptr<JsReplacer::StringRewriter> rewriter2(
      NewPermanentCallback(this, &JsReplacerTest::AppendHead));
  replacer_.AddPattern("document", "domain", rewriter.get());
  replacer_.AddPattern("document", "domain", rewriter2.get());

  GoogleString out;
  replacer_.Transform(kIn, &out);
  EXPECT_EQ(kOut, out);
}

TEST_F(JsReplacerTest, TwoPatterns) {
  // Test two different patterns.
  const char kIn[] =
      "a.b.c = \"42\"; document.domain = 'whatever.com';";
  const char kOut[] =
      "a.b.c = \"head with 42\"; document.domain = 'whatever.com with tail';";
  scoped_ptr<JsReplacer::StringRewriter> rewriter(
      NewPermanentCallback(this, &JsReplacerTest::AppendTail));
  scoped_ptr<JsReplacer::StringRewriter> rewriter2(
      NewPermanentCallback(this, &JsReplacerTest::AppendHead));
  replacer_.AddPattern("document", "domain", rewriter.get());
  replacer_.AddPattern("b", "c", rewriter2.get());

  GoogleString out;
  replacer_.Transform(kIn, &out);
  EXPECT_EQ(kOut, out);
}

TEST_F(JsReplacerTest, CommentsOk) {
  const char kIn[] =
      "a.b.c = \"42\"; document.domain = /*relax*/ 'whatever.com';";
  const char kOut[] =
      "a.b.c = \"42\"; document.domain = /*relax*/ 'whatever.com with tail';";
  scoped_ptr<JsReplacer::StringRewriter> rewriter(
      NewPermanentCallback(this, &JsReplacerTest::AppendTail));
  replacer_.AddPattern("document", "domain", rewriter.get());

  GoogleString out;
  replacer_.Transform(kIn, &out);
  EXPECT_EQ(kOut, out);
}

}  // namespace

}  // namespace net_instaweb
