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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: dpeng@google.com (Daniel Peng)

#include "webutil/css/property.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace {

class PropertyTest : public testing::Test {
};

TEST_F(PropertyTest, PropFromText) {
  string s("border-width");
  EXPECT_EQ(Css::Property::BORDER_WIDTH,
            Css::Property::PropFromText(s.c_str(), s.length()));
}

TEST_F(PropertyTest, TextFromProp) {
  EXPECT_STREQ("border-width",
               Css::Property::TextFromProp(Css::Property::BORDER_WIDTH));
}

TEST_F(PropertyTest, Inverses) {
  for(int i = 0; i < Css::Property::OTHER; ++i) {
    string s(Css::Property::TextFromProp(static_cast<Css::Property::Prop>(i)));
    EXPECT_EQ(static_cast<Css::Property::Prop>(i),
              Css::Property::PropFromText(s.c_str(), s.length()));
  }
}

}  // namespace
