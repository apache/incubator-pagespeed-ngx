// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_PARSER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_PARSER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class ResponseHeaders;

// Parses a stream of HTTP header text into a ResponseHeaders instance.
class ResponseHeadersParser {
 public:
  explicit ResponseHeadersParser(ResponseHeaders* rh) : response_headers_(rh) {
    Clear();
  }

  void Clear();

  // Parse a chunk of HTTP response header.  Returns number of bytes consumed.
  int ParseChunk(const StringPiece& text, MessageHandler* handler);

  bool headers_complete() const { return headers_complete_; }
  void set_headers_complete(bool x) { headers_complete_ = x; }

 private:
  bool GrabLastToken(const GoogleString& input, GoogleString* output);

  ResponseHeaders* response_headers_;

  bool parsing_http_;
  bool parsing_value_;
  bool headers_complete_;
  GoogleString parse_name_;
  GoogleString parse_value_;

  DISALLOW_COPY_AND_ASSIGN(ResponseHeadersParser);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_PARSER_H_
