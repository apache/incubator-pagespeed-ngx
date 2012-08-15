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

#include "net/instaweb/util/public/escaping.h"

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class EscapingTest : public testing::Test {
 public:
  void ExpectEscape(const char* name, const char* expect, const char* in) {
    GoogleString out_unquoted, out_quoted;
    EscapeToJsStringLiteral(in, false, &out_unquoted);
    EscapeToJsStringLiteral(in, true, &out_quoted);
    EXPECT_STREQ(expect, out_unquoted) << " on test " << name;
    EXPECT_STREQ(StrCat("\"", expect, "\""), out_quoted) << " on test " << name;
  }
};

TEST_F(EscapingTest, JsEscapeBasic) {
  ExpectEscape("normal", "abc", "abc");
  ExpectEscape("quote", "abc\\\"d", "abc\"d");
  ExpectEscape("backslash", "abc\\\\d", "abc\\d");
  ExpectEscape("carriage_control", "abc\\n\\rde", "abc\n\rde");
}

TEST_F(EscapingTest, JsAvoidCloseScript) {
  ExpectEscape("avoid_close_script", "Foo<\\/script>Bar", "Foo</script>Bar");
  ExpectEscape("not_heavily_excessive_escaping", "/s", "/s");
}

TEST_F(EscapingTest, JsAvoidCloseScriptSpace) {
  ExpectEscape("avoid_close_script2",
               "Foo<\\/script  >Bar", "Foo</script  >Bar");
}

TEST_F(EscapingTest, JsAvoidCloseScriptCase) {
  ExpectEscape("avoid_close_script3",
               "Foo<\\/scrIpt>Bar", "Foo</scrIpt>Bar");
}

TEST_F(EscapingTest, JsCloseScriptConservativeBehavior) {
  // We don't need to escape </scripty>, but it's safe to do so.
  ExpectEscape("close_script_conservative",
               "Foo<\\/scripty>Bar", "Foo</scripty>Bar");
}

TEST_F(EscapingTest, JsSingleQuotes) {
  GoogleString out_unquoted, out_quoted;
  const char kIn[] = "foo'";
  EscapeToJsStringLiteral(kIn, false, &out_unquoted);
  EscapeToJsStringLiteral(kIn, true, &out_quoted);
  EXPECT_STREQ("foo\\'", out_unquoted);
  EXPECT_STREQ("\"foo'\"", out_quoted);
}

}  // namespace

}  // namespace net_instaweb
