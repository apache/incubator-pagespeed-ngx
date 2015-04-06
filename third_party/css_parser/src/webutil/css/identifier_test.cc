/**
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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yi-an Huang)

#include "webutil/css/identifier.h"

#include <string>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "webutil/css/string.h"

namespace {

class IdentifierTest : public testing::Test {
};

TEST_F(IdentifierTest, IdentFromText) {
  UnicodeText s = UTF8ToUnicodeText(string("Inherit"), true);
  EXPECT_EQ(Css::Identifier::INHERIT, Css::Identifier::IdentFromText(s));
  EXPECT_EQ(Css::Identifier::INHERIT, Css::Identifier(s).ident());
  EXPECT_EQ("inherit", UnicodeTextToUTF8(Css::Identifier(s).ident_text()));
}

TEST_F(IdentifierTest, TextFromIdent) {
  EXPECT_EQ("inherit",
             UnicodeTextToUTF8(Css::Identifier::TextFromIdent(
                 Css::Identifier::INHERIT)));
  EXPECT_EQ("OTHER",
             UnicodeTextToUTF8(Css::Identifier::TextFromIdent(
                 Css::Identifier::OTHER)));
}

TEST_F(IdentifierTest, UnicodeText) {
  UnicodeText s = UTF8ToUnicodeText(string("宋体"), true);
  EXPECT_EQ(Css::Identifier::OTHER, Css::Identifier::IdentFromText(s));
  Css::Identifier id(s);
  EXPECT_EQ(Css::Identifier::OTHER, id.ident());
  EXPECT_EQ("宋体", UnicodeTextToUTF8(id.ident_text()));
}

TEST_F(IdentifierTest, Inverses) {
  for (int i = 0; i < Css::Identifier::OTHER; ++i) {
    UnicodeText s = Css::Identifier::TextFromIdent(
        static_cast<Css::Identifier::Ident>(i));
    EXPECT_EQ(static_cast<Css::Identifier::Ident>(i),
              Css::Identifier::IdentFromText(s));
  }
}

}  // namespace
