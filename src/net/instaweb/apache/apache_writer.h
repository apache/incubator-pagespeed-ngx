// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_APACHE_APACHE_WRITER_H_
#define NET_INSTAWEB_APACHE_APACHE_WRITER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "httpd.h"  // NOLINT

struct request_rec;

namespace net_instaweb {

class MessageHandler;
class ResponseHeaders;

// Writer object that writes to an Apache Request stream.
class ApacheWriter : public Writer {
 public:
  explicit ApacheWriter(request_rec* r);
  virtual ~ApacheWriter();

  virtual bool Write(const StringPiece& str, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

  // Copies the contents of the specified response_headers to the Apache
  // headers_out structure.  This must be done before any bytes are flushed.
  //
  // Note: if strip_cokies is set, the cookies will be stripped here.
  //
  // If set_content_length was previously called, this will set a
  // content length to avoid chunked encoding, otherwise it will clear
  // any content-length specified in the response headers.
  void OutputHeaders(ResponseHeaders* response_headers);
  void set_content_length(int64 x) { content_length_ = x; }

  // Disables mod_expires and mod_headers to allow the headers to
  // be under control of mod_pagespeed.  Default is false.
  void set_disable_downstream_header_filters(bool x) {
    disable_downstream_header_filters_ = x;
  }

  // Removes 'Set-Cookie' and 'Set-Cookie2' from the response headers
  // once they are complete.  Default is false.
  void set_strip_cookies(bool x) {
    strip_cookies_ = x;
  }

  // When proxying content we deem to be unsafe (e.g. lacking
  // a Content-Type header) we must squelch the output.
  void set_squelch_output(bool x) { squelch_output_ = true; }
  bool squelch_output() const { return squelch_output_; }

 private:
  request_rec* request_;
  bool headers_out_;
  bool disable_downstream_header_filters_;
  bool strip_cookies_;
  bool squelch_output_;
  int64 content_length_;

  DISALLOW_COPY_AND_ASSIGN(ApacheWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_WRITER_H_
