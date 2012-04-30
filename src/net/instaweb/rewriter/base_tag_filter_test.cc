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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class BaseTagFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kAddBaseTag);
    rewrite_driver()->AddFilters();
  }
};

TEST_F(BaseTagFilterTest, SingleHead) {
  ValidateExpected("single_head",
      "<head></head>"
      "<body><img src=\"1.jpg\" /></body>",
      "<head>"
      "<base href=\"http://test.com/single_head.html\">"
      "</head>"
      "<body><img src=\"1.jpg\"/></body>");
}

TEST_F(BaseTagFilterTest, NoHeadTag) {
  ValidateExpected("no_head",
      "<body><img src=\"1.jpg\" /></body>",
      "<head>"
      "<base href=\"http://test.com/no_head.html\">"
      "</head>"
      "<body><img src=\"1.jpg\"/></body>");
}

TEST_F(BaseTagFilterTest, MultipleHeadTags) {
  ValidateExpected("multiple_heads",
      "<head></head>"
      "<head></head>"
      "<body>"
      "</body>",
      "<head>"
      "<base href=\"http://test.com/multiple_heads.html\">"
      "</head>"
      "<head></head>"
      "<body>"
      "</body>");
}

}  // namespace net_instaweb
