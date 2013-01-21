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

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
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
  friend class SuppressPreheadFilterTest;
  void SendCookies(HtmlElement* element);

  static void UpdateFetchLatencyInFlushEarlyProto(int64 latency,
                                                  RewriteDriver*driver);

  // If X-UA-Compatible meta tag is set then convert that to response header.
  bool ExtractAndUpdateXUACompatible(HtmlElement* element);

  bool seen_first_head_;
  bool has_charset_;
  bool has_x_ua_compatible_;

  HtmlElement* noscript_element_;
  RewriteDriver* driver_;
  GoogleString pre_head_;  // The html text till the <head>
  GoogleString charset_;
  // Writer for writing to the response buffer.
  Writer* original_writer_;
  StringWriter pre_head_writer_;  // Writer to write the pre_head_.
  // Writer to write both the pre_head string and to the response buffer.
  scoped_ptr<SplitWriter> pre_head_and_response_writer_;
  ResponseHeaders response_headers_;

  DISALLOW_COPY_AND_ASSIGN(SuppressPreheadFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SUPPRESS_PREHEAD_FILTER_H_
