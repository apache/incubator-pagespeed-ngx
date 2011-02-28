// Copyright 2010 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class CssMoveToHeadFilterTest : public ResourceManagerTestBase {
 protected:
  CssMoveToHeadFilterTest()
      : move_to_head_filter_(&rewrite_driver_, NULL) {
    rewrite_driver_.AddFilter(&move_to_head_filter_);
  }

  CssMoveToHeadFilter move_to_head_filter_;
};

TEST_F(CssMoveToHeadFilterTest, MovesCssToHead) {
  static const char html_input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"  // no indent
      "  <style type='text/css'>a {color: red }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>"  // no newline
      "<style type='text/css'>a {color: red }</style>"        // no newline
      "<link rel='stylesheet' href='c.css' type='text/css'>"  // no newline
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  \n"
      "  \n"
      "  World!\n"
      "  \n"
      "</body>\n";

  ValidateExpected("move_css_to_head", html_input, expected_output);
}

TEST_F(CssMoveToHeadFilterTest, DoesntMoveOutOfNoScript) {
  static const char html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <noscript>\n"
      "    <link rel='stylesheet' href='a.css' type='text/css'>\n"
      "  </noscript>\n"
      "</body>\n";

  ValidateNoChanges("noscript", html);
}


TEST_F(CssMoveToHeadFilterTest, DoesntReorderCss) {
  static const char html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>a { color: red }</style>\n"
      "  <link rel='stylesheet' href='d.css' type='text/css'>\n"
      "</body>\n";

  Parse("no_reorder_css", html);
  LOG(INFO) << "output_buffer_ = " << output_buffer_;
  size_t a_loc = output_buffer_.find("href='a.css'");
  size_t b_loc = output_buffer_.find("href='b.css'");
  size_t c_loc = output_buffer_.find("a { color: red }");
  size_t d_loc = output_buffer_.find("href='d.css'");

  // Make sure that all attributes are in output_buffer_ ...
  EXPECT_NE(output_buffer_.npos, a_loc);
  EXPECT_NE(output_buffer_.npos, b_loc);
  EXPECT_NE(output_buffer_.npos, c_loc);
  EXPECT_NE(output_buffer_.npos, d_loc);

  // ... and that they are still in the right order (specifically, that
  // the last link wasn't moved above the style).
  EXPECT_LE(a_loc, b_loc);
  EXPECT_LE(b_loc, c_loc);
  EXPECT_LE(c_loc, d_loc);
}

}  // namespace

}  // namespace net_instaweb
