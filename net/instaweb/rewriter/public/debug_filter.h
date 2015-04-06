/*
 * Copyright 2012 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DEBUG_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DEBUG_FILTER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Timer;

// Injects HTML comments for measuring the time it takes to parse HTML,
// run the Flush/Render sequence, and the idle-time between text blocks.
// Data is written into the HTML as comments.
class DebugFilter : public EmptyHtmlFilter {
 public:
  explicit DebugFilter(RewriteDriver* driver);
  virtual ~DebugFilter();

  virtual void EndDocument();
  virtual void Flush();

  virtual const char* Name() const { return "Debug"; }

  // Special entry-points needed for measuring timing.  The timing
  // of StartDocument/EndDocument does not capture the correct timing,
  // and changing them so they do would alter functionality depended
  // upon by numerous filters.  So we have special entry-points for
  // this filter called directly by RewriteDriver.  This can be generalized
  // in the future if these entry-points prove useful.
  void InitParse();
  void StartParse();
  void EndParse();
  void StartRender();
  void EndRender();

  virtual void EndElement(HtmlElement* element);

  // Formats Flush/EndOfDocument messages that will be easy to read from
  // View->PageSource in a browser.
  //
  // They are exposed for testing, so that unit tests are not concerned with
  // the exact formatting of those messages.
  static GoogleString FormatFlushMessage(int64 time_since_init_parse_us,
                                         int64 parse_duration_us,
                                         int64 flush_duration_us,
                                         int64 idle_duration_us);
  static GoogleString FormatEndDocumentMessage(
      int64 time_since_init_parse_us, int64 total_parse_duration_us,
      int64 total_flush_duration_us, int64 total_idle_duration_us,
      int num_flushes, bool is_critical_images_beacon_enabled,
      const StringSet& critical_image_urls,
      const StringVector& dynamically_disabled_filter_list);
  // Gets the list of active filters from the RewriteDriver for logging to debug
  // message.
  GoogleString ListActiveFiltersAndOptions() const;

 private:
  // Tracks duration of events of interest that may occur multiple times
  // during an HTML rewrite.
  class Event {
   public:
    Event();
    inline void Clear();
    inline void Start(int64 now_us);
    inline void End(int64 now_us);
    inline void AddToTotal();

    int64 start_us() const { return start_us_; }
    int64 duration_us() const { return duration_us_; }
    int64 total_us() const { return total_us_; }

   private:
    int64 start_us_;
    int64 duration_us_;
    int64 total_us_;
  };

  void Clear();

  RewriteDriver* driver_;
  Timer* timer_;
  bool end_document_seen_;   // Set at EndOfDocument, checked at Flush.
  int num_flushes_;
  int64 start_doc_time_us_;  // Established at InitParse.
  Event parse_;              // Tracks how much time is spent parsing.
  Event render_;             // Tracks how much time is spent rendering.
  Event idle_;               // Tracks how much time is spent waiting.
  StringSet critical_image_urls_;

  // The buffered flush messages this filter generates for a flush in a literal
  // tag.
  GoogleString flush_messages_;

  StringVector dynamically_disabled_filter_list_;

  DISALLOW_COPY_AND_ASSIGN(DebugFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DEBUG_FILTER_H_
