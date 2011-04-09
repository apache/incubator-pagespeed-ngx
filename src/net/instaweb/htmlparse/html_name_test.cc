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

#include <set>
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/symbol_table.h"

namespace net_instaweb {

class HtmlNameTest : public testing::Test {
 protected:
};

TEST_F(HtmlNameTest, OneKeyword) {
  EXPECT_EQ(HtmlName::kStyle, HtmlName::Lookup("style"));
}

TEST_F(HtmlNameTest, AllKeywordsDefaultCase) {
  for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
    EXPECT_EQ(iter.keyword(), HtmlName::Lookup(iter.name()));
  }
}

TEST_F(HtmlNameTest, AllKeywordsUpperCase) {
  for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
    GoogleString upper(iter.name());
    UpperString(&upper);
    EXPECT_EQ(iter.keyword(), HtmlName::Lookup(upper));
  }
}

TEST_F(HtmlNameTest, AllKeywordsMixedCase) {
  for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
    GoogleString mixed(iter.name());
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
    EXPECT_EQ(iter.keyword(), HtmlName::Lookup(mixed));
  }
}

TEST_F(HtmlNameTest, Bogus) {
  EXPECT_EQ(HtmlName::kNotAKeyword, HtmlName::Lookup("hiybbprqag"));
  EXPECT_EQ(HtmlName::kNotAKeyword, HtmlName::Lookup("stylex"));
}

TEST_F(HtmlNameTest, Iterator) {
  int num_iters = 0;
  StringSet names;
  std::set<HtmlName::Keyword> keywords;
  for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
    EXPECT_GT(HtmlName::num_keywords(), static_cast<int>(iter.keyword()));
    keywords.insert(iter.keyword());
    names.insert(iter.name());
    ++num_iters;
  }
  EXPECT_EQ(HtmlName::num_keywords(), num_iters);
  EXPECT_EQ(HtmlName::num_keywords(), keywords.size());
  EXPECT_EQ(HtmlName::num_keywords(), names.size());
}

}  // namespace net_instaweb
