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

#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/mock_url_fetcher.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

MockUrlFetcher::~MockUrlFetcher() {
  STLDeleteContainerPairSecondPointers(response_map_.begin(),
                                       response_map_.end());
}

void MockUrlFetcher::SetResponse(const StringPiece& url,
                                 const MetaData& response_header,
                                 const StringPiece& response_body) {
  HttpResponse* response = new HttpResponse(response_header, response_body);
  response_map_.insert(ResponseMap::value_type(url.as_string(), response));
}

bool MockUrlFetcher::StreamingFetchUrl(const std::string& url,
                                       const MetaData& request_headers,
                                       MetaData* response_headers,
                                       Writer* response_writer,
                                       MessageHandler* message_handler) {
  bool ret = false;
  if (enabled_) {
    ResponseMap::iterator iter = response_map_.find(url);
    if (iter != response_map_.end()) {
      const HttpResponse* response = iter->second;
      response_headers->CopyFrom(response->header());
      response_writer->Write(response->body(), message_handler);
      ret = true;
    } else {
      // This is used in tests and we do not expect the test to request a
      // resource that we don't have. So fail if we do.
      //
      // If you want a 404 response, you must explicitly use SetResponse.
      message_handler->Message(kFatal, "Requested unset url %s", url.c_str());
    }
  }
  return ret;
}

}  // namespace net_instaweb
