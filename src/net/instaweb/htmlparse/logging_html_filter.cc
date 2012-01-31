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

#include "net/instaweb/htmlparse/public/logging_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/htmlparse/public/statistics_log.h"

// TODO(jmarantz): convert to Statistics interface

namespace {
// Printable names of the statistics.
// Must match up with enum Statistic in logging_html_filter.h;
// is this bad for maintenance?
const char* kStatisticNames[] = {
  "explicit_close", "implicit_close", "brief_close", "closed", "unclosed",
  "spurious_close", "tags", "cdata", "comments", "directives", "documents",
  "IE_directives",
};
}

namespace net_instaweb {

LoggingFilter::LoggingFilter() {
  Reset();
}

void LoggingFilter::StartDocument() {
  ++stats_[NUM_DOCUMENTS];
}

void LoggingFilter::StartElement(HtmlElement* element) {
  // Does EndElement get called for singleton elements?
  ++stats_[NUM_UNCLOSED];
  ++stats_[NUM_TAGS];
}

void LoggingFilter::EndElement(HtmlElement* element) {
  // Figure out what's up with the element (implicitly vs explicitly closed)
  switch (element->close_style()) {
    case HtmlElement::EXPLICIT_CLOSE: {
      --stats_[NUM_UNCLOSED];
      ++stats_[NUM_CLOSED];
      ++stats_[NUM_EXPLICIT_CLOSED];
      break;
    }
    case HtmlElement::IMPLICIT_CLOSE: {
      --stats_[NUM_UNCLOSED];
      ++stats_[NUM_CLOSED];
      ++stats_[NUM_IMPLICIT_CLOSED];
      break;
    }
    case HtmlElement::BRIEF_CLOSE: {
      --stats_[NUM_UNCLOSED];
      ++stats_[NUM_CLOSED];
      ++stats_[NUM_BRIEF_CLOSED];
      break;
    }
    case HtmlElement::UNCLOSED: {
      // We assumed unmatchedness at StartElement, so do nothing.
      break;
    }
    case HtmlElement::AUTO_CLOSE: {
      // Another form of unmated tag, again do nothing.
      break;
    }
  }
}

void LoggingFilter::Cdata(HtmlCdataNode* cdata) {
  ++stats_[NUM_CDATA];
}

void LoggingFilter::Comment(HtmlCommentNode* comment) {
  ++stats_[NUM_COMMENTS];
}

void LoggingFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  ++stats_[NUM_IE_DIRECTIVES];
}

void LoggingFilter::Directive(HtmlDirectiveNode* directive) {
  ++stats_[NUM_DIRECTIVES];
}

// Logging, diffing, and aggregation
void LoggingFilter::LogStatistics(StatisticsLog *statistics_log) const {
  for (int statistic = MIN_STAT; statistic < MAX_STAT; ++statistic) {
    statistics_log->LogStat(kStatisticNames[statistic], stats_[statistic]);
  }
}

void LoggingFilter::Reset() {
  // Cleaner than memset?
  for (int statistic = MIN_STAT; statistic < MAX_STAT; ++statistic) {
    stats_[statistic] = 0;
  }
}

}  // namespace net_instaweb
