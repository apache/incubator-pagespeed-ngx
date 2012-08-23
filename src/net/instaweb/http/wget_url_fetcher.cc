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

// Authors: jmarantz@google.com (Joshua Marantz)
//          vchudnov@google.com (Victor Chudnovsky)

#include "net/instaweb/http/public/wget_url_fetcher.h"

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

WgetUrlFetcher::WgetUrlFetcher() {
  set_binary("/usr/bin/wget");
}

const char* WgetUrlFetcher::GetFetchLabel() {
  return "wget";
}


GoogleString WgetUrlFetcher::ConstructFetchCommand(
    const GoogleString& escaped_url,
    const char* user_agent,
    const StringVector& escaped_headers) {

  GoogleString cmd(binary_);
  StrAppend(&cmd, " --save-headers -q -O -");

  // Use default user-agent if none is set in headers.
  if (user_agent == NULL) {
    StrAppend(&cmd,
              " --user-agent \"",
              ExternalUrlFetcher::kDefaultUserAgent,
              "\"");
  }

  for (StringVector::const_iterator it = escaped_headers.begin();
       it != escaped_headers.end();
       ++it) {
    StrAppend(&cmd, " --header \"", *it, "\"");
  }

  StrAppend(&cmd, " \"", escaped_url, "\"");

  return cmd;
}

}  // namespace net_instaweb
