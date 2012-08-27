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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/blink_util.h"

#include "base/logging.h"               // for operator<<, etc
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {

namespace {

class BlinkUtilTest : public ::testing::Test {
};

TEST_F(BlinkUtilTest, ClearArrayIfAllEmpty) {
  Json::Value val(Json::arrayValue);
  Json::Value val_obj(Json::objectValue);
  val_obj["contiguous"] = "blah";

  val.append(val_obj);
  val.append(val_obj);
  val.append(val_obj);

  BlinkUtil::ClearArrayIfAllEmpty(&val);
  EXPECT_EQ(0, val.size());

  val.append(val_obj);

  val_obj["instance_html"] = "blah";
  val.append(val_obj);
  BlinkUtil::ClearArrayIfAllEmpty(&val);
  EXPECT_EQ(2, val.size());
}

TEST_F(BlinkUtilTest, IsJsonEmpty) {
  Json::Value val_obj(Json::objectValue);
  EXPECT_TRUE(BlinkUtil::IsJsonEmpty(val_obj));
  val_obj["contiguous"] = "blah";

  EXPECT_TRUE(BlinkUtil::IsJsonEmpty(val_obj));

  val_obj["instance_html"] = "blah";
  EXPECT_FALSE(BlinkUtil::IsJsonEmpty(val_obj));
}

TEST_F(BlinkUtilTest, EscapeString) {
  GoogleString str1 = "<stuff\xe2\x80\xa8>\n\\n";
  BlinkUtil::EscapeString(&str1);
  EXPECT_EQ("__psa_lt;stuff\\u2028__psa_gt;\n\\n", str1);
  GoogleString str2 = "<|  |\\n";  // Has couple of U+2028's betwen the |
  BlinkUtil::EscapeString(&str2);
  EXPECT_EQ("__psa_lt;|\\u2028\\u2028|\\n", str2);
}

}  // namespace
}  // namespace net_instaweb
