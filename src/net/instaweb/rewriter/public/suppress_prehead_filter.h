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
// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SUPPRESS_PREHEAD_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SUPPRESS_PREHEAD_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/split_writer.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Writer;

// SuppressPreheadFilter extracts the html before the <head> (pre head) and
// stores it in property cache to be used by FlushEarlyFlow. If a request is
// flushed early then the pre head value stored by this filter is used for
// responding to the request. When a response is received by the origin server,
// then this filter suppresses the pre head so that it is not written to the
// output again.
class SuppressPreheadFilter : public HtmlWriterFilter {
 public:
  explicit SuppressPreheadFilter(RewriteDriver* driver);

  virtual void StartDocument();

  virtual void StartElement(HtmlElement* element);

  virtual void EndElement(HtmlElement* element);

  virtual void EndDocument();

 protected:
  virtual void Clear();

 private:
  bool seen_first_head_;
  HtmlElement* noscript_element_;
  HtmlElement* meta_tag_element_;
  RewriteDriver* driver_;
  GoogleString pre_head_;  // The html text till the <head>
  GoogleString content_type_meta_tag_;
  // Writer for writing to the response buffer.
  Writer* original_writer_;
  // The writer before we saw the meta tag.
  Writer* pre_meta_tag_writer_;
  StringWriter pre_head_writer_;  // Writer to write the pre_head_.
  StringWriter content_type_meta_tag_writer_;
  NullWriter null_writer_;
  // Writer to write both the pre_head string and to the response buffer.
  scoped_ptr<SplitWriter> pre_head_and_response_writer_;
  scoped_ptr<SplitWriter> content_type_meta_tag_and_response_writer_;

  DISALLOW_COPY_AND_ASSIGN(SuppressPreheadFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SUPPRESS_PREHEAD_FILTER_H_
