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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// CollectFlushEarlyContentFilter extracts the html for non-inlined resources
// that we want to flush early
// and stores it in property cache to be used by FlushEarlyFlow. If a request is
// flushed early then this HTML is used to make the client download resources
// early.
class CollectFlushEarlyContentFilter : public HtmlWriterFilter {
 public:
  explicit CollectFlushEarlyContentFilter(RewriteDriver* driver);

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

 protected:
  virtual void Clear();

 private:
  // Are we inside a <head> node.
  bool in_head_;
  HtmlElement* current_element_;
  HtmlElement* no_script_element_;
  RewriteDriver* driver_;
  NullWriter null_writer_;  // Null writer that ignores all writes.
  GoogleString resource_html_;  // The html text containing resource elements.
  StringWriter resource_html_writer_;  // Write to write the resource_html_.

  DISALLOW_COPY_AND_ASSIGN(CollectFlushEarlyContentFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_
