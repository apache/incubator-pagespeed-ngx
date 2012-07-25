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

// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/http/public/semantic_type.h"

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class SemanticTypeTest : public testing::Test {};

TEST_F(SemanticTypeTest, TestParseCategory) {
  semantic_type::Category category;
  EXPECT_TRUE(semantic_type::ParseCategory("image", &category));
  EXPECT_EQ(semantic_type::kImage, category);
  // Check case-insensitivity.
  EXPECT_TRUE(semantic_type::ParseCategory("iMaGe", &category));
  EXPECT_EQ(semantic_type::kImage, category);
  EXPECT_TRUE(semantic_type::ParseCategory("script", &category));
  EXPECT_EQ(semantic_type::kScript, category);
  EXPECT_TRUE(semantic_type::ParseCategory("stylesheet", &category));
  EXPECT_EQ(semantic_type::kStylesheet, category);
  EXPECT_TRUE(semantic_type::ParseCategory("OtherResource", &category));
  EXPECT_EQ(semantic_type::kOtherResource, category);
  EXPECT_TRUE(semantic_type::ParseCategory("Hyperlink", &category));
  EXPECT_EQ(semantic_type::kHyperlink, category);
  EXPECT_FALSE(semantic_type::ParseCategory("", &category));
  EXPECT_FALSE(semantic_type::ParseCategory("Undefined", &category));
}

}  // namespace net_instaweb
