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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class ThreadSystem;

class SimpleStatsVariable : public Variable {
 public:
  explicit SimpleStatsVariable(AbstractMutex* mutex);
  virtual ~SimpleStatsVariable();
  virtual int64 Get() const;
  virtual void Set(int64 value);
  virtual int64 Add(int delta);
  virtual StringPiece GetName() const { return StringPiece(NULL); }

 private:
  int64 value_;
  scoped_ptr<AbstractMutex> mutex_;
  DISALLOW_COPY_AND_ASSIGN(SimpleStatsVariable);
};

// Simple name/value pair statistics implementation.
class SimpleStats : public ScalarStatisticsTemplate<SimpleStatsVariable> {
 public:
  SimpleStats();
  virtual ~SimpleStats();

 protected:
  virtual SimpleStatsVariable* NewVariable(const StringPiece& name, int index);

 private:
  scoped_ptr<ThreadSystem> thread_system_;

  DISALLOW_COPY_AND_ASSIGN(SimpleStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_STATS_H_
