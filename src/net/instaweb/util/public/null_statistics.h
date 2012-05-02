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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class NullStatisticsVariable : public Variable {
 public:
  NullStatisticsVariable() {}
  virtual ~NullStatisticsVariable();
  virtual int Get() const { return 0; }
  virtual void Set(int value) { }
  virtual int64 Get64() const { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatisticsVariable);
};

// Simple name/value pair statistics implementation.
class NullStatistics : public ScalarStatisticsTemplate<NullStatisticsVariable> {
 public:
  static const int kNotFound;

  NullStatistics() { }
  virtual ~NullStatistics();

 protected:
  virtual NullStatisticsVariable* NewVariable(const StringPiece& name,
                                              int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatistics);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_
