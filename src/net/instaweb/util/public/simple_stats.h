/*
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_STATS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_STATS_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class SimpleStatsVariable : public Variable {
 public:
  SimpleStatsVariable() : value_(0) {}
  virtual ~SimpleStatsVariable();
  virtual int Get() const { return value_; }
  virtual int64 Get64() const { return value_; }
  virtual void Set(int value) { value_ = value; }

 private:
  int value_;

  DISALLOW_COPY_AND_ASSIGN(SimpleStatsVariable);
};

// Simple name/value pair statistics implementation.
class SimpleStats : public StatisticsTemplate<SimpleStatsVariable,
                                              NullStatisticsHistogram,
                                              FakeTimedVariable> {
 public:
  static const int kNotFound;

  SimpleStats() { }
  virtual ~SimpleStats();

 protected:
  virtual SimpleStatsVariable* NewVariable(const StringPiece& name, int index);
  virtual NullStatisticsHistogram* NewHistogram();
  virtual FakeTimedVariable* NewTimedVariable(const StringPiece& name,
                                              int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_STATS_H_
