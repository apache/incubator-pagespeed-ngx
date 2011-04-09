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

#include "net/instaweb/http/public/response_headers_parser.h"

#include <stdio.h>
#include "base/logging.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

void ResponseHeadersParser::Clear() {
  parsing_http_ = false;
  parsing_value_ = false;
  headers_complete_ = false;
}

// TODO(jmaessen): http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
// I bet we're doing this wrong:
//  Header fields can be extended over multiple lines by preceding each extra
//  line with at least one SP or HT.
int ResponseHeadersParser::ParseChunk(const StringPiece& text,
                                      MessageHandler* handler) {
  CHECK(!headers_complete_);
  int num_consumed = 0;
  int num_bytes = text.size();

  for (; num_consumed < num_bytes; ++num_consumed) {
    char c = text[num_consumed];
    if ((c == '/') && (parse_name_ == "HTTP")) {
      if (response_headers_->has_major_version()) {
        handler->Message(kError, "Multiple HTTP Lines");
      } else {
        parsing_http_ = true;
        parsing_value_ = true;
      }
    } else if (!parsing_value_ && (c == ':')) {
      parsing_value_ = true;
    } else if (c == '\r') {
      // Just ignore CRs for now, and break up headers on newlines for
      // simplicity.  It's not clear to me if it's important that we
      // reject headers that lack the CR in front of the LF.
    } else if (c == '\n') {
      if (parse_name_.empty()) {
        // blank line.  This marks the end of the headers.
        ++num_consumed;
        headers_complete_ = true;
        response_headers_->ComputeCaching();
        break;
      }
      if (parsing_http_) {
        // Parsing "1.0 200 OK\r", using sscanf for the integers, and
        // private method GrabLastToken for the "OK".
        int major_version, minor_version, status_code;
        GoogleString reason_phrase;
        if ((sscanf(parse_value_.c_str(), "%d.%d %d ",  // NOLINT
                    &major_version, &minor_version, &status_code) != 3) ||
            !GrabLastToken(parse_value_, &reason_phrase)) {
          // TODO(jmarantz): capture the filename/url, track the line numbers.
          handler->Message(kError, "Invalid HTML headers: %s",
                           parse_value_.c_str());
        } else {
          response_headers_->set_major_version(major_version);
          response_headers_->set_minor_version(minor_version);
          response_headers_->set_status_code(status_code);
          response_headers_->set_reason_phrase(reason_phrase);
        }
        parsing_http_ = false;
      } else {
        response_headers_->Add(parse_name_, parse_value_);
      }
      parsing_value_ = false;
      parse_name_.clear();
      parse_value_.clear();
    } else if (parsing_value_) {
      // Skip leading whitespace
      if (!parse_value_.empty() || !isspace(c)) {
        parse_value_ += c;
      }
    } else {
      parse_name_ += c;
    }
  }
  return num_consumed;
}

// Grabs the last non-whitespace token from 'input' and puts it in 'output'.
bool ResponseHeadersParser::GrabLastToken(const GoogleString& input,
                                          GoogleString* output) {
  bool ret = false;
  // Safely grab the response code string from the end of parse_value_.
  int last_token_char = -1;
  for (int i = input.size() - 1; i >= 0; --i) {
    char c = input[i];
    if (isspace(c)) {
      if (last_token_char >= 0) {
        // We found the whole token.
        const char* token_start = input.c_str() + i + 1;
        int token_len = last_token_char - i;
        output->append(token_start, token_len);
        ret = true;
        break;
      }
    } else if (last_token_char == -1) {
      last_token_char = i;
    }
  }
  return ret;
}

}  // namespace net_instaweb
