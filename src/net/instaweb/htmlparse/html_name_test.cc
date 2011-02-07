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

// Unit-test the html name class, make sure we can do case
// insensitive matching.

#include "net/instaweb/htmlparse/public/html_name.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/symbol_table.h"

namespace net_instaweb {

class HtmlNameTest : public testing::Test {
 protected:
  HtmlName::Keyword Parse(const char* str) {
    Atom atom = symbol_table_.Intern(str);
    HtmlName name(atom);
    return name.keyword();
  }

  int num_sorted_pairs() const { return HtmlName::num_sorted_pairs(); }
  const HtmlName::NameKeywordPair* sorted_pairs() const {
    return HtmlName::sorted_pairs();
  }

  SymbolTableSensitive symbol_table_;
};

TEST_F(HtmlNameTest, OneKeyword) {
  EXPECT_EQ(HtmlName::kStyle, Parse("style"));
}

TEST_F(HtmlNameTest, AllKeywordsDefaultCase) {
  for (int i = 0; i < num_sorted_pairs(); ++i) {
    const HtmlName::NameKeywordPair& nkp = sorted_pairs()[i];
    EXPECT_EQ(nkp.keyword, Parse(nkp.name));
  }
}

TEST_F(HtmlNameTest, AllKeywordsUpperCase) {
  for (int i = 0; i < num_sorted_pairs(); ++i) {
    const HtmlName::NameKeywordPair& nkp = sorted_pairs()[i];
    std::string upper(nkp.name);
    UpperString(&upper);
    EXPECT_EQ(nkp.keyword, Parse(upper.c_str()));
  }
}

TEST_F(HtmlNameTest, AllKeywordsMixedCase) {
  for (int i = 0; i < num_sorted_pairs(); ++i) {
    const HtmlName::NameKeywordPair& nkp = sorted_pairs()[i];
    std::string mixed(nkp.name);
    bool upper = false;
    for (int i = 0, n = mixed.size(); i < n; ++i) {
      char c = mixed[i];
      upper = !upper;
      if (upper) {
        c = UpperChar(c);
      } else {
        c = LowerChar(c);
      }
      mixed[i] = c;
    }
    EXPECT_EQ(nkp.keyword, Parse(mixed.c_str()));
  }
}

TEST_F(HtmlNameTest, Bogus) {
  EXPECT_EQ(HtmlName::kNotAKeyword, Parse("hiybbprqag"));
  EXPECT_EQ(HtmlName::kNotAKeyword, Parse("stylex"));  // close to 'style'
}

}  // namespace net_instaweb
