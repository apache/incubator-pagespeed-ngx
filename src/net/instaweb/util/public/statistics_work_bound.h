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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_WORK_BOUND_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_WORK_BOUND_H_

#include "net/instaweb/util/public/work_bound.h"

namespace net_instaweb {

class Variable;

// A WorkBound implementation in terms of statistics.  This is a bit of a hack
// that gets things implemented quickly (especially given the complexity of
// multiprocess shared-memory infrastructure, which we only want to roll once).
// Note in particular that we handle a NULL variable gracefully by imposing no
// bound at all.
class StatisticsWorkBound : public WorkBound {
 public:
  // Note that ownership of variable remains with the creating Statistics
  // object.  If the bound is 0, the bound is actually infinite.
  StatisticsWorkBound(Variable* variable, int bound);
  virtual ~StatisticsWorkBound();

  virtual bool TryToWork();
  virtual void WorkComplete();
 private:
  Variable* variable_;
  int bound_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_WORK_BOUND_H_
