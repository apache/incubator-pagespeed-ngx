/**
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/dummy_url_fetcher.h"

#include "net/instaweb/util/public/message_handler.h"
#include <string>

namespace net_instaweb {

bool DummyUrlFetcher::StreamingFetchUrl(const std::string& url,
                                        const RequestHeaders& request_headers,
                                        ResponseHeaders* response_headers,
                                        Writer* fetched_content_writer,
                                        MessageHandler* message_handler) {
  message_handler->Message(kFatal, "DummyUrlFetcher used");
  return false;
}

}  // namespace net_instaweb
