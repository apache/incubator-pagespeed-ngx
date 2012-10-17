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

#include <list>

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class HtmlElement;
class RewriteDriver;
class TimedVariable;
class Writer;

struct ResourceInfo;

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

  // Check whether resource can be flushed or not.
  bool IsFlushable(const GoogleUrl& gurl, bool is_pagespeed_resource);

  // Flush the resource and update time_consumed_ms_ based on time_to_download.
  void FlushResources(
      StringPiece url,
      int64 time_to_download,
      bool is_pagespeed_resource,
      semantic_type::Category category);

  RewriteDriver* driver_;
  TimedVariable* num_resources_flushed_early_;
  // Whether we need to insert a close script tag at EndDocument.
  bool in_body_;
  bool insert_close_script_;
  int num_resources_flushed_;
  NullWriter null_writer_;
  Writer* original_writer_;
  HtmlElement* current_element_;
  UserAgentMatcher::PrefetchMechanism prefetch_mechanism_;
  scoped_ptr<StringSet> private_cacheable_resources_;
  int64 time_consumed_ms_;
  int64 max_available_time_ms_;
  typedef std::list<ResourceInfo*> ResourceInfoList;
  ResourceInfoList js_resources_info_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyContentWriterFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_CONTENT_WRITER_FILTER_H_
