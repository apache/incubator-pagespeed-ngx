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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/http/public/wget_url_fetcher.h"

#include <cerrno>
#include "net/instaweb/http/public/http_response_parser.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/writer.h"

namespace {

// It turns out to be harder to quote in bash with single-quote
// than double-quote.  From man sh:
//
//   Single Quotes
//     Enclosing characters in single quotes preserves the literal meaning of
//     all the characters (except single quotes, making it impossible to put
//     single-quotes in a single-quoted string).
//
//   Double Quotes
//     Enclosing characters within double quotes preserves the literal meaning
//     of all characters except dollarsign ($), backquote (‘), and backslash
//     (\).  The backslash inside double quotes is historically weird, and
//     serves to quote only the following characters:
//           $ ‘ " \ <newline>.
//     Otherwise it remains literal.
//
// So we put double-quotes around most strings, after first escaping
// any of these characters:
const char kEscapeChars[] = "\"$`\\";
}

namespace net_instaweb {

// Default user agent to a Chrome user agent, so that we get real website.
const char WgetUrlFetcher::kDefaultUserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; en-US) "
    "AppleWebKit/534.0 (KHTML, like Gecko) Chrome/6.0.408.1 Safari/534.0";

WgetUrlFetcher::~WgetUrlFetcher() {
}

bool WgetUrlFetcher::StreamingFetchUrl(const GoogleString& url,
                                       const RequestHeaders& request_headers,
                                       ResponseHeaders* response_headers,
                                       Writer* writer,
                                       MessageHandler* handler) {
  GoogleString cmd("/usr/bin/wget --save-headers -q -O -"), escaped_url;

  // Use default user-agent if none is set in headers.
  StringStarVector values;
  request_headers.Lookup("user-agent", &values);
  if (values.empty()) {
    cmd += StrCat(" --user-agent=\"", kDefaultUserAgent, "\"");
  }

  for (int i = 0, n = request_headers.NumAttributes(); i < n; ++i) {
    GoogleString escaped_name, escaped_value;

    BackslashEscape(request_headers.Name(i), kEscapeChars, &escaped_name);
    BackslashEscape(request_headers.Value(i), kEscapeChars, &escaped_value);
    cmd += StrCat(" --header=\"", escaped_name, ": ", escaped_value, "\"");
  }

  BackslashEscape(url, kEscapeChars, &escaped_url);
  cmd += StrCat(" \"", escaped_url, "\"");
  handler->Message(kInfo, "wget %s\n", url.c_str());
  FILE* wget_stdout = popen(cmd.c_str(), "r");

  bool ret = false;
  if (wget_stdout == NULL) {
    handler->Message(kError, "Wget popen failed on url %s: %s",
                     url.c_str(), strerror(errno));
  } else {
    HttpResponseParser parser(response_headers, writer, handler);
    ret = parser.Parse(wget_stdout);
    int exit_status = pclose(wget_stdout);
    if (exit_status != 0) {
      // The wget failed.  wget does not always (ever?) write appropriate
      // headers when it fails, so invent some.
      if (response_headers->status_code() == 0) {
        response_headers->set_first_line(1, 1, HttpStatus::kBadRequest,
                                         "Wget Failed");
        response_headers->ComputeCaching();
        // TODO(jmarantz): set_headers_complete
        // response_headers->set_headers_complete(true);
        writer->Write("wget failed: ", handler);
        writer->Write(url, handler);
        writer->Write("<br>\nExit Status: ", handler);
        writer->Write(IntegerToString(exit_status), handler);
      }
    }
  }
  return ret;
}

}  // namespace net_instaweb
