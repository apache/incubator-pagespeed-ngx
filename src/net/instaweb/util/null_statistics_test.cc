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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/null_statistics.h"

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

// Make sure none of the Get* functions crash for NullStatistics.
TEST(NullStatisticsTest, CanGetAllVarTypes) {
  NullStatistics stats;

  stats.AddVariable("var");
  Variable* var = stats.GetVariable("var");
  var->Add(1);
  var->Add(-5);

  stats.AddHistogram("hist");
  Histogram* hist = stats.GetHistogram("hist");
  hist->Add(1.0);
  hist->Add(-2.14159);  // Steve's pi.

  stats.AddTimedVariable("timed_var", "group");
  TimedVariable* timed_var = stats.GetTimedVariable("timed_var");
  timed_var->IncBy(1);
  timed_var->IncBy(-13);
}

}  // namespace

}  // namespace net_instaweb
