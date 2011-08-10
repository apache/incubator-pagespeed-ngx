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

#include "net/instaweb/util/public/waveform.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;

Waveform::Waveform(ThreadSystem* thread_system, Timer* timer, int capacity)
    : timer_(timer),
      capacity_(capacity),
      samples_(new TimeValue[capacity]),
      mutex_(thread_system->NewMutex()) {
  Clear();
}

void Waveform::Clear() {
  ScopedMutex lock(mutex_.get());
  start_index_ = 0;
  size_ = 0;
  first_sample_timestamp_us_ = 0;
  total_since_clear_ = 0.0;
  min_ = 0.0;
  max_ = 0.0;
}

int Waveform::Size() {
  // TODO(jmarantz): use reader-lock.
  ScopedMutex lock(mutex_.get());
  return size_;
}

double Waveform::Average() {
  // TODO(jmarantz): use reader-lock.
  ScopedMutex lock(mutex_.get());
  if (size_ == 0) {
    return 0.0;
  }

  // We could make the average by looking at the delta from
  // timer_->NowUs(), rather than prev->first.  But with an active
  // server this won't make much difference, and with a server being
  // debugged I think it's better like this:  the time between the
  // first event and the last event.
  TimeValue* prev = GetSample(size_ - 1);
  int64 elapsed_us = prev->first - first_sample_timestamp_us_;
  return (total_since_clear_ / elapsed_us);
}

double Waveform::Minimum() {
  // TODO(jmarantz): use reader-lock.
  ScopedMutex lock(mutex_.get());
  return min_;
}

double Waveform::Maximum() {
  // TODO(jmarantz): use reader-lock.
  ScopedMutex lock(mutex_.get());
  return max_;
}

Waveform::TimeValue* Waveform::GetSample(int index) {
  // Must be called with mutex held.
  DCHECK_LE(0, index);
  DCHECK_GT(size_, index);
  return &samples_[(start_index_ + index) % capacity_];
}

void Waveform::Add(double value) {
  // TODO(jmarantz): use writer-lock.
  ScopedMutex lock(mutex_.get());
  int64 now_us = timer_->NowUs();
  if (size_ == 0) {
    max_ = value;
    min_ = value;
    first_sample_timestamp_us_ = now_us;
  } else {
    TimeValue* prev = GetSample(size_ - 1);
    int64 elapsed_us = now_us - prev->first;
    total_since_clear_ += elapsed_us * prev->second;  // time-weighted values
    if (value < min_) {
      min_ = value;
    } else if (value > max_) {
      max_ = value;
    }
  }

  if (size_ == capacity_) {
    start_index_ = (start_index_ + 1) % capacity_;
  } else {
    ++size_;
  }
  TimeValue* tv = GetSample(size_ - 1);
  tv->first = now_us;
  tv->second = value;
}

// See http://code.google.com/apis/chart/interactive/docs/gallery/linechart.html
namespace {

const char kChartApiLoad[] =
    "<script type='text/javascript' src='https://www.google.com/jsapi'>"
    "</script>\n"
    "<script type='text/javascript'>\n"
    "  google.load('visualization', '1', {packages:['corechart']});\n"
    "  google.setOnLoadCallback(drawWaveforms);\n"
    "  var google_waveforms = new Array();\n"
    "  function drawWaveform(title, id, legend, points) {\n"
    "    var data = new google.visualization.DataTable();\n"
    "    data.addColumn('number', 'Time (ms)');\n"
    "    data.addColumn('number', legend);\n"
    "    data.addRows(points.length);\n"
    "    var min_x = 0;\n"
    "    var max_x = 0;\n"
    "    var min_y = 0;\n"
    "    var max_y = 0;\n"
    "    for (var i = 0; i < points.length; ++i) {\n"
    "      var point = points[i];\n"
    "      var x = point[0];\n"
    "      var y = point[1];\n"
    "      if ((i == 0) || (x < min_x)) { min_x = x; }\n"
    "      if ((i == 0) || (x > max_x)) { max_x = x; }\n"
    "      if ((i == 0) || (y < min_y)) { min_y = y; }\n"
    "      if ((i == 0) || (y > max_y)) { max_y = y; }\n"
    "      data.setValue(i, 0, x);\n"
    "      data.setValue(i, 1, y);\n"
    "    }\n"
    "    var chart = new google.visualization.ScatterChart(\n"
    "        document.getElementById(id));\n"
    "    chart.draw(data, {\n"
    "        width: 800, height: 480, title: title, legend: 'none',\n"
    "        hAxis: {title: 'time (ms)', minValue: min_x, "
    "maxValue: 1.1 * max_x},\n"
    "        vAxis: {minValue: min_y, maxValue: 1.1 * "
    "max_y}});\n"
    "  }\n"
    "  function drawWaveforms() {\n"
    "    for (var i = 0; i < google_waveforms.length; ++i) {\n"
    "      var w = google_waveforms[i];\n"
    "      w();\n"
    "    }\n"
    "  }\n"
    "  function addWaveform(title, id, legend, points) {\n"
    "    google_waveforms.push(function() {drawWaveform(title, id, legend, "
    "points);});\n"
    "  }\n"
    "</script>";

const char kChartWaveformPrefixFormat[] =
    "<script type='text/javascript'>\n"
    "  addWaveform('%s', '%s', '%s', [\n";  // title, id, legend

const char kSampleFormat[] =
    "    [%f, %f],\n";

const char kWaveformSuffixFormat[] =
    "]);\n"
    "</script>\n"
    "<div id='%s'></div>\n";

}  // namespace

void Waveform::RenderHeader(Writer* writer, MessageHandler* handler) {
  writer->Write(kChartApiLoad, handler);
}

void Waveform::Render(const StringPiece& title, const StringPiece& label,
                      Writer* writer, MessageHandler* handler) {
  ScopedMutex lock(mutex_.get());
  if (size_ == 0) {
    writer->Write(StrCat(title, ": no data"), handler);
  } else {
    TimeValue* tv = GetSample(0);
    int64 start_time_us = tv->first;

    MD5Hasher hasher;
    GoogleString div_id = hasher.Hash(title);

    writer->Write(StringPrintf(kChartWaveformPrefixFormat,
                               title.as_string().c_str(),
                               div_id.c_str(),
                               label.as_string().c_str()),
                  handler);

    for (int i = 0; i < size_; ++i) {
      tv = GetSample(i);
      int64 delta_us = tv->first - start_time_us;
      writer->Write(StringPrintf(kSampleFormat, delta_us / 1000.0,
                                 static_cast<double>(tv->second)),
                    handler);
    }

    writer->Write(StringPrintf(kWaveformSuffixFormat, div_id.c_str()),
                  handler);
  }
}

}  // namespace net_instaweb
