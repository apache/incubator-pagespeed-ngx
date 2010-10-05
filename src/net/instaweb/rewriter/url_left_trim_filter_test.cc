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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/url_left_trim_filter.h"

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class UrlLeftTrimFilterTest : public HtmlParseTestBase {
 protected:
  UrlLeftTrimFilterTest()
      : left_trim_filter_(&html_parse_, NULL) {
    html_parse_.AddFilter(&left_trim_filter_);
  }

  void AddTrimming(const StringPiece& trimming) {
    left_trim_filter_.AddTrimming(trimming);
  }

  void OneTrim(bool changed,
               const StringPiece init, const StringPiece expected) {
    StringPiece url(init);
    EXPECT_EQ(changed, left_trim_filter_.Trim(&url));
    EXPECT_EQ(expected, url);
  }

  virtual bool AddBody() const { return false; }

  UrlLeftTrimFilter left_trim_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlLeftTrimFilterTest);
};

static const char kBase[] = "http://foo.bar/baz/";
static const char kHttp[] = "http:";
static const char kDomain[] = "//foo.bar";
static const char kPath[] = "/baz/";

TEST_F(UrlLeftTrimFilterTest, SimpleTrims) {
  StringPiece spHttp(kHttp);
  StringPiece spDomain(kDomain);
  StringPiece spPath(kPath);
  AddTrimming(spHttp);
  AddTrimming(spDomain);
  AddTrimming(spPath);
  OneTrim(true, "http://www.google.com/", "//www.google.com/");
  OneTrim(true, kBase, kPath);
  OneTrim(true, "http://foo.bar/baz/quux", "quux");
  OneTrim(true, "/baz/quux", "quux");
  OneTrim(true, "//foo.bar/img/img1.jpg", "/img/img1.jpg");
  OneTrim(false, "/img/img1.jpg", "/img/img1.jpg");
  OneTrim(false, kHttp, kHttp);
  OneTrim(true, "//foo.bar/baz/quux", "quux");
}

static const char kRootedBase[] = "http://foo.bar/";

// Catch screw cases when a base url lies at the root of a domain.
TEST_F(UrlLeftTrimFilterTest, RootedTrims) {
  left_trim_filter_.AddBaseUrl(kRootedBase);
  OneTrim(true, "http://www.google.com/", "//www.google.com/");
  OneTrim(true, kBase, kPath);
  OneTrim(false, "//www.google.com/", "//www.google.com/");
  OneTrim(false, kPath, kPath);
  OneTrim(false, "quux", "quux");
}

static const char kNone[] =
    "<head><base href='ftp://what.the/heck/'/>"
    "<link src='ftp://what.the/heck/'></head>"
    "<body><a href='spdy://www.google.com/'>google</a>"
    "<img src='file:///where/the/heck.jpg'/></body>";

TEST_F(UrlLeftTrimFilterTest, NoChanges) {
  left_trim_filter_.AddBaseUrl(kBase);
  ValidateNoChanges("none forward", kNone);
}

static const char kSome[] =
    "<head><base href='http://foo.bar/baz/'/>"
    "<link src='http://foo.bar/baz/'></head>"
    "<body><a href='http://www.google.com/'>google</a>"
    "<img src='http://foo.bar/baz/nav.jpg'/>"
    "<img src='http://foo.bar/img/img1.jpg'/>"
    "<img src='/baz/img2.jpg'/>"
    "<img src='//foo.bar/baz/widget.png'/></body>";

static const char kSomeRewritten[] =
    "<head><base href='http://foo.bar/baz/'/>"
    "<link src='/baz/'></head>"
    "<body><a href='//www.google.com/'>google</a>"
    "<img src='nav.jpg'/>"
    "<img src='/img/img1.jpg'/>"
    "<img src='img2.jpg'/>"
    "<img src='widget.png'/></body>";

TEST_F(UrlLeftTrimFilterTest, SomeChanges) {
  left_trim_filter_.AddBaseUrl(kBase);
  ValidateExpected("some forward", kSome, kSomeRewritten);
}

}  // namespace net_instaweb
