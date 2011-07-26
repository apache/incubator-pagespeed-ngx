/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jhoch@google.com (Jason R. Hoch)

#include "net/instaweb/rewriter/public/div_structure_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class DivStructureFilterTest : public HtmlParseTestBase {
 protected:
  DivStructureFilterTest()
      : div_structure_filter_() {
    html_parse_.AddFilter(&div_structure_filter_);
  }

  virtual bool AddBody() const { return false; }

  private:
    DivStructureFilter div_structure_filter_;
};

// TODO(jhoch): Make tests agnostic to query param value encoding.
TEST_F(DivStructureFilterTest, NoDivTest) {
  static const char html_input[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "  <p>Today's top stories are:</p>\n"
      "  <ol>\n"
      "    <li><a href=\"http://www.example1.com\">"
               "Website wins award for most boring URL.</a></li>\n"
      "    <li><a href=\"http://www.example2.com\">"
               "Copycats quickly try to steal some spotlight.</a></li>\n"
      "    <li><a href=\"http://www.example3.com\">Internet proves itself "
               "capable of spawning copycat copycats.</a></li>\n"
      "    <li><a href=\"http://www.example5.com\">Embarrassed imitator "
               "ruins trend.</a></li>\n"
      "  </ol>\n"
      "  </body>\n"
      "</html>\n";
  static const char html_expected_output[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "  <p>Today's top stories are:</p>\n"
      "  <ol>\n"
      "    <li><a href=\"http://www.example1.com/?div_location=0\">"
               "Website wins award for most boring URL.</a></li>\n"
      "    <li><a href=\"http://www.example2.com/?div_location=1\">"
               "Copycats quickly try to steal some spotlight.</a></li>\n"
      "    <li><a href=\"http://www.example3.com/?div_location=2\">Internet "
               "proves itself capable of spawning copycat copycats.</a></li>\n"
      "    <li><a href=\"http://www.example5.com/?div_location=3\">Embarrassed "
               "imitator ruins trend.</a></li>\n"
      "  </ol>\n"
      "  </body>\n"
      "</html>\n";
  ValidateExpected("no_div_test", html_input, html_expected_output);
}

TEST_F(DivStructureFilterTest, NoHrefTest) {
  static const char html_input[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "  I guess people do this:\n"
      "  <a onclick=\"function();\">\n"
      "  </body>\n"
      "</html>\n";
  ValidateNoChanges("no_href_test", html_input);
}

TEST_F(DivStructureFilterTest, WithDivsTest) {
  static const char html_input[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"menu\">\n"
      "      <a href=\"http://www.example.com/home\">HOME</a>\n"
      "      <a href=\"http://www.example.com/contact_us\">CONTACT US</a>\n"
      "      <a href=\"http://www.example.com/about\">ABOUT</a>\n"
      "    </div>\n"
      "    <div id=\"content\">\n"
      "      <div class=\"top_story\">\n"
      "        <a href=\"http://www.example.com/top_story.txt\">TOP STORY</a>\n"
      "      </div>\n"
      "      <div class=\"stories\">\n"
      "        <a href=\"http://www.example.com/story1.html\">STORY ONE</a>\n"
      "        <a href=\"http://www.example.com/story2.html\">STORY TWO</a>\n"
      "        <a href=\"http://www.example.com/story3.html\">STORY THREE</a>\n"
      "      </div>\n"
      "    </div>\n"
      "  </body>\n"
      "</html>\n";
  static const char html_expected_output[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"menu\">\n"
      "      <a href=\"http://www.example.com/home?div_location=0.0\">"
                 "HOME</a>\n"
      "      <a href=\"http://www.example.com/contact_us?div_location=0.1\">"
                 "CONTACT US</a>\n"
      "      <a href=\"http://www.example.com/about?div_location=0.2\">"
                 "ABOUT</a>\n"
      "    </div>\n"
      "    <div id=\"content\">\n"
      "      <div class=\"top_story\">\n"
      "        <a href=\"http://www.example.com/top_story.txt"
                   "?div_location=1.0.0\">TOP STORY</a>\n"
      "      </div>\n"
      "      <div class=\"stories\">\n"
      "        <a href=\"http://www.example.com/story1.html"
                   "?div_location=1.1.0\">STORY ONE</a>\n"
      "        <a href=\"http://www.example.com/story2.html"
                   "?div_location=1.1.1\">STORY TWO</a>\n"
      "        <a href=\"http://www.example.com/story3.html"
                   "?div_location=1.1.2\">STORY THREE</a>\n"
      "      </div>\n"
      "    </div>\n"
      "  </body>\n"
      "</html>\n";
  ValidateExpected("with_divs_test", html_input, html_expected_output);
}

TEST_F(DivStructureFilterTest, TwoDigitDivCountTest) {
  static const char html_input[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"menu\">\n"
      "      <a href=\"http://www.example.com/link1\">Link 1</a>\n"
      "      <a href=\"http://www.example.com/link2\">Link 2</a>\n"
      "      <a href=\"http://www.example.com/link3\">Link 3</a>\n"
      "      <a href=\"http://www.example.com/link4\">Link 4</a>\n"
      "      <a href=\"http://www.example.com/link5\">Link 5</a>\n"
      "      <a href=\"http://www.example.com/link6\">Link 6</a>\n"
      "      <a href=\"http://www.example.com/link7\">Link 7</a>\n"
      "      <a href=\"http://www.example.com/link8\">Link 8</a>\n"
      "      <a href=\"http://www.example.com/link9\">Link 9</a>\n"
      "      <a href=\"http://www.example.com/link10\">Link 10</a>\n"
      "      <a href=\"http://www.example.com/link11\">Link 11</a>\n"
      "    </div>\n"
      "    <div id=\"content\">\n"
      "      This page contains a large menu of links.\n"
      "    </div>\n"
      "  </body>\n"
      "</html>\n";
  static const char html_expected_output[] =
      "<html>\n"
      "  <head>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"menu\">\n"
      "      <a href=\"http://www.example.com/link1?div_location=0.0\">"
                 "Link 1</a>\n"
      "      <a href=\"http://www.example.com/link2?div_location=0.1\">"
                 "Link 2</a>\n"
      "      <a href=\"http://www.example.com/link3?div_location=0.2\">"
                 "Link 3</a>\n"
      "      <a href=\"http://www.example.com/link4?div_location=0.3\">"
                 "Link 4</a>\n"
      "      <a href=\"http://www.example.com/link5?div_location=0.4\">"
                 "Link 5</a>\n"
      "      <a href=\"http://www.example.com/link6?div_location=0.5\">"
                 "Link 6</a>\n"
      "      <a href=\"http://www.example.com/link7?div_location=0.6\">"
                 "Link 7</a>\n"
      "      <a href=\"http://www.example.com/link8?div_location=0.7\">"
                 "Link 8</a>\n"
      "      <a href=\"http://www.example.com/link9?div_location=0.8\">"
                 "Link 9</a>\n"
      "      <a href=\"http://www.example.com/link10?div_location=0.9\">"
                 "Link 10</a>\n"
      "      <a href=\"http://www.example.com/link11?div_location=0.10\">"
                 "Link 11</a>\n"
      "    </div>\n"
      "    <div id=\"content\">\n"
      "      This page contains a large menu of links.\n"
      "    </div>\n"
      "  </body>\n"
      "</html>\n";
  ValidateExpected("with_divs_test", html_input, html_expected_output);
}

}  // namespace

}  // namespace net_instaweb
