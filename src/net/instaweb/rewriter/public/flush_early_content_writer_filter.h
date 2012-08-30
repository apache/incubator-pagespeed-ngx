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
// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_CONTENT_WRITER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_CONTENT_WRITER_FILTER_H_

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class TimedVariable;
class Writer;

// FlushEarlyContentWriterFilter finds rewritten resources in the DOM and
// inserts HTML that makes the browser download them. Note that we set a
// NullWriter as the writer for this driver, and directly  write whatever we
// need to the original writer.
class FlushEarlyContentWriterFilter : public HtmlWriterFilter {
 public:
  static const char kPrefetchLinkRelSubresourceHtml[];
  static const char kPrefetchImageTagHtml[];
  static const char kPrefetchStartTimeScript[];
  static const char kNumResourcesFlushedEarly[];
  static const char kPrefetchScriptTagHtml[];
  static const char kPrefetchLinkTagHtml[];

  explicit FlushEarlyContentWriterFilter(RewriteDriver* driver);

  virtual void StartDocument();
  virtual void EndDocument();

  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

 protected:
  virtual void Clear();

 private:
  // Writes the string to original_writer_.
  void WriteToOriginalWriter(const GoogleString& in);

  RewriteDriver* driver_;
  TimedVariable* num_resources_flushed_early_;
  // Whether we need to insert a close script tag at EndDocument.
  bool insert_close_script_;
  int num_resources_flushed_;
  NullWriter null_writer_;
  Writer* original_writer_;
  HtmlElement* current_element_;
  UserAgentMatcher::PrefetchMechanism prefetch_mechanism_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyContentWriterFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_CONTENT_WRITER_FILTER_H_
