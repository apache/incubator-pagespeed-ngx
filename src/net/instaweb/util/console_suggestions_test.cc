// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/console_suggestions.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

class ConsoleSuggestionsTest : public ::testing::Test {
 protected:
  ConsoleSuggestionsTest() : suggestions_factory_(&stats_) {
  }

  virtual ~ConsoleSuggestionsTest() {
  }

  SimpleStats stats_;
  ConsoleSuggestionsFactory suggestions_factory_;
};

TEST_F(ConsoleSuggestionsTest, Stats) {
  const char var1_name[] = "test_variable";
  const char var2_name[] = "another_variable";
  Variable* var1 = stats_.AddVariable(var1_name);
  Variable* var2 = stats_.AddVariable(var2_name);

  // Everything starts off at 0.
  EXPECT_EQ(0, suggestions_factory_.StatValue(var1_name));
  EXPECT_EQ(0, suggestions_factory_.StatValue(var2_name));
  // Note that we return 0 when denominator is 0.
  EXPECT_DOUBLE_EQ(0, suggestions_factory_.StatRatio(var1_name, var2_name));
  EXPECT_DOUBLE_EQ(0, suggestions_factory_.StatSumRatio(var1_name, var2_name));

  var1->Add(1);
  EXPECT_EQ(1, suggestions_factory_.StatValue(var1_name));
  EXPECT_EQ(0, suggestions_factory_.StatValue(var2_name));
  // 1 / 0 -> 0
  EXPECT_DOUBLE_EQ(0, suggestions_factory_.StatRatio(var1_name, var2_name));
  // 1 / 1 -> 1
  EXPECT_DOUBLE_EQ(1, suggestions_factory_.StatSumRatio(var1_name, var2_name));

  var1->Add(1);
  var2->Add(10);
  EXPECT_EQ(2, suggestions_factory_.StatValue(var1_name));
  EXPECT_EQ(10, suggestions_factory_.StatValue(var2_name));
  EXPECT_DOUBLE_EQ(2./10, suggestions_factory_.StatRatio(var1_name, var2_name));
  EXPECT_DOUBLE_EQ(2./12, suggestions_factory_.StatSumRatio(var1_name,
                                                            var2_name));
}

}  // namespace net_instaweb
