/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WAVEFORM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WAVEFORM_H_

#include <utility>                      // for pair

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class MessageHandler;
class ThreadSystem;
class Timer;
class Writer;

// Displays a waveform of values over time.  This can run
// continuously, in which case it will only display waveforms for a
// bounded number of samples.  Or it can be run as a trigger.
//
// However the average, min, and max values will account for all the
// values seen by the waveform since it was cleared.
//
// This class is threadsafe.
class Waveform {
 public:
  Waveform(ThreadSystem* thread_system, Timer* timer, int capacity);

  void Clear();
  double Average();
  double Maximum();
  double Minimum();
  int Size();

  // Records a value at the current time using the Timer.
  void Add(double value);

  // Records a delta relative to the previous value using the Timer.
  void AddDelta(double value);

  // Write script and function to web page. Note that this function
  // should be called only once for each HTML page, and should not be
  // used for each waveform.
  static void RenderHeader(Writer* writer, MessageHandler* handler);

  // Renders a waveform into HTML.
  void Render(const StringPiece& title, const StringPiece& label,
              Writer* writer, MessageHandler* handler);

 private:
  typedef std::pair<int64, double> TimeValue;

  TimeValue* GetSample(int index);  // Must be called with mutex held.
  void AddHelper(double value);     // Must be called with mutex held.

  Timer* timer_;
  int capacity_;
  scoped_array<TimeValue> samples_;
  int start_index_;
  int size_;
  int64 first_sample_timestamp_us_;
  double total_since_clear_;
  double min_;
  double max_;
  double previous_value_;
  scoped_ptr<AbstractMutex> mutex_;  // protects all the above member variables.

  DISALLOW_COPY_AND_ASSIGN(Waveform);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WAVEFORM_H_
