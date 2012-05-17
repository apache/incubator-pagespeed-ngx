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

#include <map>
#include <utility>
#include <vector>

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace {
// As we do fix size buckets, each bucket has the same height.
const double kBarHeightPerBucket = 20;
// Each bucket has different width, depends on the percentage of bucket value
// out of total counts.
// The width of a bucket is percentage_of_bucket_value * kBarWidthTotal.
const double kBarWidthTotal = 400;
}  // namespace
namespace net_instaweb {

class MessageHandler;

Variable::~Variable() {
}

Histogram::~Histogram() {
}

NullHistogram::~NullHistogram() {
}

TimedVariable::~TimedVariable() {
}

FakeTimedVariable::~FakeTimedVariable() {
}

void Histogram::WriteRawHistogramData(Writer* writer, MessageHandler* handler) {
  const char bucket_style[] = "<tr><td style=\"padding: 0 0 0 0.25em\">"
      "[</td><td style=\"text-align:right;padding:0 0.25em 0 0\">"
      "%.0f,</td><td style=text-align:right;padding: 0 0.25em\">%.f)</td>";
  const char value_style[] = "<td style=\"text-align:right;padding:0 0.25em\">"
                             "%.f</td>";
  const char perc_style[] = "<td style=\"text-align:right;padding:0 0.25em\">"
                            "%.1f%%</td>";
  const char bar_style[] = "<td><div style=\"width: %.fpx;height:%.fpx;"
                           "background-color:blue\"></div></td>";
  double count = CountInternal();
  double perc = 0;
  double cumulative_perc = 0;
  // Write prefix of the table.
  writer->Write("<hr><table>", handler);
  for (int i = 0, n = MaxBuckets(); i < n; ++i) {
    double value = BucketCount(i);
    if (value == 0) {
      // We do not draw empty bucket.
      continue;
    }
    double lower_bound = BucketStart(i);
    double upper_bound = BucketLimit(i);
    perc = value * 100 / count;
    cumulative_perc += perc;
    GoogleString output = StrCat(
        StringPrintf(bucket_style, lower_bound, upper_bound),
        StringPrintf(value_style, value),
        StringPrintf(perc_style, perc),
        StringPrintf(perc_style, cumulative_perc),
        StringPrintf(bar_style, (perc * kBarWidthTotal) / 100,
                     kBarHeightPerBucket));
    writer->Write(output, handler);
  }
  // Write suffix of the table.
  writer->Write("</table></div></div></div><hr style='clear:both;'/>",
                handler);
}

void Histogram::Render(const StringPiece& title,
                       Writer* writer, MessageHandler* handler) {
  ScopedMutex hold(lock());
  MD5Hasher hasher;
  // Generate an id for the histogram graph.
  GoogleString div_id = hasher.Hash(title);
  GoogleString id = StrCat("id", div_id);
  // Title of the histogram graph.
  const GoogleString title_string = StrCat("<div><h3>", title, "</h3>",
                                           "<div style='float:left;'></div>");
  // statistics numbers under graph.
  const GoogleString stat = StringPrintf("<hr/>Count: %.1f | Avg: %.1f "
      "| StdDev: %.1f | Min: %.0f | Median: %.0f | Max: %.0f "
      "| 90%%: %.0f | 95%%: %.0f | 99%%: %.0f",
      CountInternal(), AverageInternal(), StandardDeviationInternal(),
      MinimumInternal(), PercentileInternal(50), MaximumInternal(),
      PercentileInternal(90), PercentileInternal(95), PercentileInternal(99));

  const GoogleString raw_data_header = StringPrintf("<div>"
      "<span style='cursor:pointer;' onclick=\"toggleVisible('%s')\">"
      "&gt;Raw Histogram Data...</span>"
      "<div id='%s' style='display:none;'>", id.c_str(),
      id.c_str());
  GoogleString output = StrCat(title_string, raw_data_header, stat);
  // Write title, header and statistics.
  writer->Write(output, handler);
  // Write raw data table.
  WriteRawHistogramData(writer, handler);
}

Statistics::~Statistics() {
}

FakeTimedVariable* Statistics::NewFakeTimedVariable(
    const StringPiece& name, int index) {
  return new FakeTimedVariable(AddVariable(name));
}

void Statistics::RenderHistograms(Writer* writer, MessageHandler* handler) {
  // Write script for the web page.
  writer->Write("<script>\n"
      "function toggleVisible(id) {\n"
      "  var e = document.getElementById(id);\n"
      "  e.style.display = (e.style.display == '') ? 'none' : '';\n"
      "}\n</script>\n", handler);
  // Write data of each histogram.
  Histogram* hist = NULL;
  StringVector hist_names = HistogramNames();
  for (int i = 0, n = hist_names.size(); i < n; ++i) {
    hist = FindHistogram(hist_names[i]);
    hist->Render(hist_names[i], writer, handler);
  }
}

void Statistics::RenderTimedVariables(Writer* writer,
                                      MessageHandler* message_handler) {
  TimedVariable* timedvar = NULL;
  const GoogleString end("</table>\n<td>\n<td>\n");
  std::map<GoogleString, StringVector> group_map = TimedVariableMap();
  std::map<GoogleString, StringVector>::const_iterator p;
  // Export statistics in each group in one table.
  for (p = group_map.begin(); p != group_map.end(); ++p) {
    // Write table header for each group.
    const GoogleString begin = StrCat(
        "<p><table bgcolor=#eeeeff width=100%%>",
        "<tr align=center><td><font size=+2>", p->first,
        "</font></td></tr></table>",
        "</p>\n<td>\n<td>\n<td>\n<td>\n<td>\n",
        "<table bgcolor=#fff5ee frame=box cellspacing=1 cellpadding=2>\n",
        "<tr bgcolor=#eee5de><td>"
        "<form action=\"/statusz/reset\" method = \"post\">"
        "<input type=\"submit\" value = \"Reset Statistics\"></form></td>"
        "<th align=right>TenSec</th><th align=right>Minute</th>"
        "<th align=right>Hour</th><th align=right>Total</th></tr>");
    writer->Write(begin, message_handler);
    // Write each statistic as a row in the table.
    for (int i = 0, n = p->second.size(); i < n; ++i) {
      timedvar = FindTimedVariable(p->second[i]);
      const GoogleString content = StringPrintf("<tr><td> %s </td>"
          "<td align=right> %s </td><td align=right> %s </td>"
          "<td align=right> %s </td><td align=right> %s </td></tr>",
      p->second[i].c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::TENSEC)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::MINUTE)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::HOUR)).c_str(),
      Integer64ToString(timedvar->Get(TimedVariable::START)).c_str());
      writer->Write(content, message_handler);
    }
    // Write table ending part.
    writer->Write(end, message_handler);
  }
}


}  // namespace net_instaweb
