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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WORK_BOUND_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WORK_BOUND_H_

namespace net_instaweb {

// A WorkBound represents permission to do work bounded by some upper bound.
// Roughly speaking we can represent this as a bounded shared counter, but
// how we realize the counter implementation must vary from system to system.
class WorkBound {
 public:
  virtual ~WorkBound();
  virtual bool TryToWork() = 0;
  virtual void WorkComplete() = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WORK_BOUND_H_
