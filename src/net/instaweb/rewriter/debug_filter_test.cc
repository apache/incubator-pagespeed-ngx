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

#include "net/instaweb/rewriter/public/debug_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const char kScriptFormat[] = "<script src='%s'></script>";
const char kScript[] = "x.js";

class DebugFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kDebug);
    options()->EnableFilter(RewriteOptions::kExtendCacheScripts);
    rewrite_driver()->AddFilters();
    SetupWriter();
  }

  void ExtractFlushMessagesFromOutput(StringPiece code_to_erase,
                                      StringPieceVector* flush_messages) {
    // Test we got the flush buffers we expect.
    //
    // output_buffer_ now contains something like:
    //   "<token><!--xxx--><token><!--yyy-->"
    // And we want to convert that into a StringVector with "xxx" and "yyy".
    // So we rip out "<token>" and "-->" and use "<!--" as a delimiter for
    // splitting the string.
    GlobalReplaceSubstring(code_to_erase, "", &output_buffer_);
    GlobalReplaceSubstring("-->", "", &output_buffer_);
    SplitStringUsingSubstr(output_buffer_, "<!--", flush_messages);
  }

  void ParseAndMaybeFlushTwice(bool do_flush,
                               StringPieceVector* flush_messages) {
    const char kHtmlToken[] = "<token>";
    rewrite_driver()->StartParse(kTestDomain);
    AdvanceTimeUs(1);
    rewrite_driver()->ParseText(kHtmlToken);
    AdvanceTimeUs(10);                  // 11us elapsed so far.
    if (do_flush) {
      rewrite_driver()->Flush();
    }
    AdvanceTimeUs(100);                 // 111us elapsed so far.
    rewrite_driver()->ParseText(kHtmlToken);
    AdvanceTimeUs(1000);                // 1111us elapsed so far.
    if (do_flush) {
      rewrite_driver()->Flush();
    }
    AdvanceTimeUs(10000);               // 11111us elapsed so far.
    rewrite_driver()->ParseText(kHtmlToken);
    AdvanceTimeUs(100000);              // 111111us elapsed so far.
    rewrite_driver()->FinishParse();

    ExtractFlushMessagesFromOutput(kHtmlToken, flush_messages);
  }

  GoogleString OptScriptHtml() {
    return StringPrintf(kScriptFormat,
                        Encode(kTestDomain, "ce", "0", kScript, "js").c_str());
  }

  void InitiateScriptRewrite() {
    rewrite_driver()->StartParse(kTestDomain);
    rewrite_driver()->ParseText(StringPrintf(kScriptFormat, kScript));
  }

  void RewriteScriptToWarmTheCache() {
    // Cache-extend a simple JS file.  Then slow down the metadata-cache
    // lookup so that the Flush takes non-zero time.
    SetResponseWithDefaultHeaders("x.js", kContentTypeJavascript, "x=0", 100);

    // First, rewrite the HTML with no cache delays.
    InitiateScriptRewrite();
    rewrite_driver()->FinishParse();
    StringPieceVector flush_messages;
    ExtractFlushMessagesFromOutput(OptScriptHtml(), &flush_messages);
    ASSERT_EQ(1, flush_messages.size());
    EXPECT_EQ(DebugFilter::FormatEndDocumentMessage(0, 0, 0, 0, 0),
              flush_messages[0]);

    // Clear the output buffer as the bytes would otherwise accumulate.
    output_buffer_.clear();
  }

  int64 InjectCacheDelay() {
    // Now rewrite the image but make the cache take non-zero time so we measure
    // elapsed time for the Flush.  We stay within the deadline.
    const int64 deadline_us = rewrite_driver()->rewrite_deadline_ms() *
        Timer::kMsUs;
    const int64 delay_us = deadline_us / 3;
    SetCacheDelayUs(delay_us);
    return delay_us;
  }
};

// Tests a simple flow for a parse with two intervening flushes and delays.
// Note that our "HTML" is just "<token>", so that we can easily split the
// output and examine each flush-buffer individually.
TEST_F(DebugFilterTest, TwoFlushes) {
  StringPieceVector flush_messages;
  ParseAndMaybeFlushTwice(true, &flush_messages);

  // Note that we get no parse-time or flush time in this test.  I don't know
  // how to inject parse-time as we have no mock-time-advancement mechanism in
  // the parser flow.  We'll test that we can count flush-time in the test
  // below.  What we measure in this test is elapsed time, and idle time
  // in between the flushes.
  //
  // There are just two flushes but we get 3 flush messages, to
  // separately account for the 3 chunks of text before, between, and
  // after the flushes, plus one EndOfDocument message.
  ASSERT_EQ(4, flush_messages.size());
  EXPECT_EQ(DebugFilter::FormatFlushMessage(11, 0, 0, 11),
            flush_messages[0]);
  EXPECT_EQ(DebugFilter::FormatFlushMessage(1111, 0, 0, 1100),
            flush_messages[1]);
  EXPECT_EQ(DebugFilter::FormatFlushMessage(111111, 0, 0, 110000),
            flush_messages[2]);
  EXPECT_EQ(DebugFilter::FormatEndDocumentMessage(111111, 0, 0, 111111, 2),
            flush_messages[3]);
}

// This is the same exact test, except that Flush is not called; despite
// the elapsed time between parse chunks.  The EndDocument message will
// be the same, but there will be no Flush messages; not even one at the
// end.
TEST_F(DebugFilterTest, ZeroFlushes) {
  StringPieceVector flush_messages;
  ParseAndMaybeFlushTwice(false, &flush_messages);

  // The totals are identical to DebugFilterTest.TwoFlushes, but there are
  // no Flush messages (not even 1 at the end), and the flush-count is 0 rather
  // than 2.
  ASSERT_EQ(1, flush_messages.size());
  EXPECT_EQ(DebugFilter::FormatEndDocumentMessage(111111, 0, 0, 111111, 0),
            flush_messages[0]);
}

TEST_F(DebugFilterTest, FlushWithDelayedCache) {
  RewriteScriptToWarmTheCache();
  int64 delay_us = InjectCacheDelay();
  InitiateScriptRewrite();

  // Flush before finishing the parse.  The delay is accounted for in the
  // first Flush, and there will be a second Flush which won't do anything,
  // followed by the summary data for the rewrite at EndDocument.
  rewrite_driver()->Flush();
  rewrite_driver()->FinishParse();
  StringPieceVector flush_messages;
  ExtractFlushMessagesFromOutput(OptScriptHtml(), &flush_messages);
  ASSERT_EQ(3, flush_messages.size());
  EXPECT_EQ(DebugFilter::FormatFlushMessage(0, 0, delay_us, 0),
            flush_messages[0]);
  EXPECT_EQ(DebugFilter::FormatFlushMessage(delay_us, 0, 0, 0),
            flush_messages[1]);
  EXPECT_EQ(DebugFilter::FormatEndDocumentMessage(delay_us, 0, delay_us, 0, 1),
            flush_messages[2]);
}

TEST_F(DebugFilterTest, EndWithDelayedCache) {
  RewriteScriptToWarmTheCache();
  int64 delay_us = InjectCacheDelay();
  InitiateScriptRewrite();

  // Finish the parse immediately, which causes an implicit Flush.  However
  // since there's only one, the report is dropped as everything is in the
  // EndDocument.
  rewrite_driver()->FinishParse();
  StringPieceVector flush_messages;
  ExtractFlushMessagesFromOutput(OptScriptHtml(), &flush_messages);
  ASSERT_EQ(1, flush_messages.size());
  EXPECT_EQ(DebugFilter::FormatEndDocumentMessage(0, 0, delay_us, 0, 0),
            flush_messages[0]);
}

TEST_F(DebugFilterTest, FlushInStyleTag) {
  // Verify that flush comments do not get insert in the middle of a literal tag
  // (style or script) and instead are buffered until the end of that element.
  const char kStyleStartTag[] = "<style>";
  const char kStyleEndTag[] = "</style>";
  const char kCss1[] = ".a { color:red; }";
  const char kCss2[] = ".b { color:blue; }";
  rewrite_driver()->StartParse(kTestDomain);
  AdvanceTimeUs(1);
  rewrite_driver()->ParseText(kStyleStartTag);
  rewrite_driver()->ParseText(kCss1);
  AdvanceTimeUs(10);                  // 11us elapsed so far.
  rewrite_driver()->Flush();
  AdvanceTimeUs(10);                  // 21us elapsed so far.
  rewrite_driver()->ParseText(kCss2);
  AdvanceTimeUs(10);                  // 31us elapsed so far.
  rewrite_driver()->Flush();
  AdvanceTimeUs(10);                  // 41us elapsed so far.
  rewrite_driver()->ParseText(kStyleEndTag);
  AdvanceTimeUs(10);                  // 51us elapsed so far.
  rewrite_driver()->FinishParse();
  EXPECT_STREQ(
      StrCat(
          StrCat("<!--",
                 DebugFilter::FormatFlushMessage(11, 0, 0, 11),
                 "-->"),
          kStyleStartTag,
          kCss1,
          kCss2,
          kStyleEndTag,
          StrCat("<!--",
                 DebugFilter::FormatFlushMessage(31, 0, 0, 20),
                 "-->"),
          StrCat("<!--",
                 DebugFilter::FormatFlushMessage(51, 0, 0, 20),
                 "-->"),
          StrCat("<!--",
                 DebugFilter::FormatEndDocumentMessage(51, 0, 0, 51, 2),
                 "-->")),
      output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
