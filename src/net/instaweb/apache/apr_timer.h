// Copyright 2010 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#ifndef HTML_REWRITER_APR_TIMER_H_
#define HTML_REWRITER_APR_TIMER_H_

#include "net/instaweb/util/public/timer.h"

using net_instaweb::Timer;

namespace net_instaweb {

class AprTimer : public Timer {
 public:
  virtual ~AprTimer();
  virtual int64 NowUs() const;
  virtual void SleepUs(int64 us);
};

}  // namespace net_instaweb

#endif  // HTML_REWRITER_APR_TIMER_H_
