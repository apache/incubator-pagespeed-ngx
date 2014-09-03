// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/http/public/http_response_parser.h"

#include "net/instaweb/http/public/fetcher_test.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class HttpResponseParserTest : public FetcherTest {
};

TEST_F(HttpResponseParserTest, TestFetcher) {
  GoogleString http_filename = TestFilename();
  GoogleString http, content;
  ResponseHeaders response_headers;
  MockTimer timer(new NullMutex, 0);
  StdioFileSystem file_system;
  ASSERT_TRUE(file_system.ReadFile(http_filename.c_str(), &http,
                                   &message_handler_));
  StringWriter writer(&content);
  HttpResponseParser fetch(&response_headers, &writer, &message_handler_);
  ASSERT_TRUE(fetch.ParseChunk(http));
  ValidateOutput(content, response_headers);
}

}  // namespace net_instaweb
