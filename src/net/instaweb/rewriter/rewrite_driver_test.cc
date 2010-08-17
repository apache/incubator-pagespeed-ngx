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

// Author: jmarantz@google.com (Joshua D. Marantz)

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/dummy_url_fetcher.h"
#include "net/instaweb/util/public/fake_url_async_fetcher.h"
#include "net/instaweb/util/public/stdio_file_system.h"

namespace net_instaweb {

class RewriteDriverTest : public HtmlParseTestBaseNoAlloc {
 protected:
  RewriteDriverTest() : dummy_url_async_fetcher_(&dummy_url_fetcher_),
                        rewrite_driver_(&message_handler_, &file_system_,
                                        &dummy_url_async_fetcher_) {
  }

  virtual bool AddBody() const { return false; }
  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

 private:
  DummyUrlFetcher dummy_url_fetcher_;
  FakeUrlAsyncFetcher dummy_url_async_fetcher_;
  StdioFileSystem file_system_;
  RewriteDriver rewrite_driver_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverTest);
};

TEST_F(RewriteDriverTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

}  // namespace net_instaweb
