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

// Unit-tests for HtmlDetector

#include "net/instaweb/automatic/public/html_detector.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

class HtmlDetectorTest : public testing::Test {
 public:
  HtmlDetectorTest() {}

 protected:
  HtmlDetector html_detector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlDetectorTest);
};

TEST_F(HtmlDetectorTest, DetectHtml) {
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.ConsiderInput("  <html>"));
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.probable_html());
  GoogleString buf;
  html_detector_.ReleaseBuffered(&buf);
  EXPECT_TRUE(buf.empty());
}

TEST_F(HtmlDetectorTest, DetectHtmlBOM) {
  // Make sure utf-8 BOMs don't scare us away.
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.ConsiderInput("\xEF\xBB\xBF<html>"));
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.probable_html());
  GoogleString buf;
  html_detector_.ReleaseBuffered(&buf);
  EXPECT_TRUE(buf.empty());
}

TEST_F(HtmlDetectorTest, DetectJS) {
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.ConsiderInput("  var content_type='wrong';"));
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.probable_html());
  GoogleString buf;
  html_detector_.ReleaseBuffered(&buf);
  EXPECT_TRUE(buf.empty());
}

TEST_F(HtmlDetectorTest, BufferedHtml) {
  // Test to make sure that if there isn't enough to decide initially
  // that the content is buffered properly.
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.ConsiderInput("\t\t"));
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.ConsiderInput("  "));
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.ConsiderInput("  <html>"));
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.probable_html());
  GoogleString buf;
  html_detector_.ReleaseBuffered(&buf);
  EXPECT_EQ(buf, "\t\t  ");
}

TEST_F(HtmlDetectorTest, BufferedJs) {
  // Test to make sure that if there isn't enough to decide initially
  // that the content is buffered properly.
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.ConsiderInput("\t\t"));
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.ConsiderInput("  "));
  EXPECT_FALSE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.ConsiderInput("  var x = 42;"));
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.probable_html());
  GoogleString buf;
  html_detector_.ReleaseBuffered(&buf);
  EXPECT_EQ(buf, "\t\t  ");
}

TEST_F(HtmlDetectorTest, ForceDecisionTrue) {
  EXPECT_FALSE(html_detector_.already_decided());
  html_detector_.ForceDecision(true);
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_TRUE(html_detector_.probable_html());
}

TEST_F(HtmlDetectorTest, ForceDecisionFalse) {
  EXPECT_FALSE(html_detector_.already_decided());
  html_detector_.ForceDecision(false);
  EXPECT_TRUE(html_detector_.already_decided());
  EXPECT_FALSE(html_detector_.probable_html());
}

}  // namespace

}  // namespace net_instaweb
