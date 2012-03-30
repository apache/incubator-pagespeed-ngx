/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/htmlparse/public/canonical_attributes.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CanonicalAttributesTest : public HtmlParseTestBase {
 protected:
  CanonicalAttributesTest() : canonical_attributes_(&html_parse_) {
    html_parse_.AddFilter(&canonical_attributes_);
  }

  virtual bool AddBody() const { return false; }

  GoogleString Image(const StringPiece& image) {
    return StrCat("<img src='", image, "'>");
  }

  CanonicalAttributes canonical_attributes_;
};

TEST_F(CanonicalAttributesTest, Unescaped) {
  ValidateExpected(
      "unescaped",
      Image("a.png?a=b&c=d"),
      Image("a.png?a=b&amp;c=d"));
  EXPECT_EQ(1, canonical_attributes_.num_changes());
}

TEST_F(CanonicalAttributesTest, Escaped) {
  ValidateNoChanges(
      "escaped",
      Image("a.png?a=b&amp;c=d"));
}

TEST_F(CanonicalAttributesTest, QueryWithEncodedAmpersand) {
  // Mixed usage of unterminated & followed by a well-formed &amp;.  We
  // correct the usage here.
  ValidateExpected(
      "ampersand",
      Image("discuss/a.php?&action=vtopic&amp;forum=2"),
      Image("discuss/a.php?&amp;action=vtopic&amp;forum=2"));
}

TEST_F(CanonicalAttributesTest, Numeric) {
  ValidateNoChanges(
      "numeric_escape",
      "<a title='&#8217;'>b</a>");
}

TEST_F(CanonicalAttributesTest, NonUtf8) {
  // The input &#250; is transformed to its symbolic form &ucacute;.
  ValidateExpected(
      "non_utf8",
      "<a title='&#250;'>b</a>",
      "<a title='&uacute;'>b</a>");
  EXPECT_EQ(1, canonical_attributes_.num_changes());
  EXPECT_EQ(0, canonical_attributes_.num_errors());
}

TEST_F(CanonicalAttributesTest, Spanish) {
  // The 8-bit input cannot be processed; we consider it a decoding error, so
  // we live the text alone.
  ValidateNoChanges("spanish", "<a title='muñecos'>b</a>");
  EXPECT_EQ(0, canonical_attributes_.num_changes());
  EXPECT_EQ(1, canonical_attributes_.num_errors());
}

TEST_F(CanonicalAttributesTest, AccentedSingleByteEscape) {
  // We can transfer single-byte escapes into 8-bit and back without loss.
  ValidateNoChanges("spanish", "<a title='&atilde;&Yacute;'>b</a>");
  EXPECT_EQ(1, canonical_attributes_.num_changes());
  EXPECT_EQ(0, canonical_attributes_.num_errors());
}

//
// TODO(jmarantz): fix handling of empty attribute names.
// TEST_F(CanonicalAttributesTest, EmptyAttrName) {
//   ValidateNoChanges("empty_attr_name", "<img  ='109'/>");
// }

TEST_F(CanonicalAttributesTest, Nasa) {
  ValidateNoChanges("nasa", "<a title='NASA’s Budget'>b</a>");
}

TEST_F(CanonicalAttributesTest, Retronaut) {
  ValidateNoChanges("retronaut",
                    "<link title='Retronaut &raquo; Feed'/>");
}

TEST_F(CanonicalAttributesTest, SingleQuoteInAttr) {
  // This is fully valid, and we rewrite the attribute, but no textual
  // change takes place.
  ValidateNoChanges("squote", "<link title=\"a&#39;s b &raquo; Feed\">");
  EXPECT_EQ(1, canonical_attributes_.num_changes());
  EXPECT_EQ(0, canonical_attributes_.num_errors());
}

TEST_F(CanonicalAttributesTest, Hellip) {
  // &hellip; exists, but would need to be unescaped as multi-byte so we do
  // not process it.
  ValidateNoChanges("hellip", "<input value='Search this website &hellip;'/>");
  EXPECT_EQ(0, canonical_attributes_.num_changes());
  EXPECT_EQ(1, canonical_attributes_.num_errors());
}

TEST_F(CanonicalAttributesTest, Yuml) {
  // &Yuml; & &yuml; both exist, but &Yuml; is multi-byte, so we error out.
  // &yuml; is single-byte, so we process it properly.
  ValidateNoChanges("Yuml", "<input value='Search this website &Yuml;'/>");
  EXPECT_EQ(0, canonical_attributes_.num_changes());
  EXPECT_EQ(1, canonical_attributes_.num_errors());
  ValidateNoChanges("yuml", "<input value='Search this website &yuml;'/>");
  EXPECT_EQ(1, canonical_attributes_.num_changes());
  EXPECT_EQ(0, canonical_attributes_.num_errors());
}

TEST_F(CanonicalAttributesTest, Truncated) {
  // Here we "correct" the missing ";" in the input.
  ValidateExpected("truncated",
                   "<link href='foo.css?user=z&amp'/>",
                   "<link href='foo.css?user=z&amp;'/>");
}

TEST_F(CanonicalAttributesTest, EndsWithAmpersand) {
  // Here we "correct" the missing "amp;" in the input.
  ValidateExpected("ends_with_ampersand",
                   "<link href='foo.css?user=z&'/>",
                   "<link href='foo.css?user=z&amp;'/>");
}

TEST_F(CanonicalAttributesTest, EndsWithValue) {
  // Here we "correct" the input, transforming & to &amp;.
  ValidateExpected("ends",
                   "<link href='a/b?c=d&e=a&t'>",
                   "<link href='a/b?c=d&amp;e=a&amp;t'>");
}

}  // namespace net_instaweb
