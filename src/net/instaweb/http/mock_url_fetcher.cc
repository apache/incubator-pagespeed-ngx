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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/mock_url_fetcher.h"

#include <map>
#include <utility>                      // for pair
#include "base/logging.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;

MockUrlFetcher::~MockUrlFetcher() {
  Clear();
}

void MockUrlFetcher::SetResponse(const StringPiece& url,
                                 const ResponseHeaders& response_header,
                                 const StringPiece& response_body) {
  // Note: This is a little kludgey, but if you set a normal response and
  // always perform normal GETs you won't even notice that we've set the
  // last_modified_time internally.
  SetConditionalResponse(url, 0, response_header, response_body);
}

void MockUrlFetcher::AddToResponse(const StringPiece& url,
                                   const StringPiece& name,
                                   const StringPiece& value) {
  ResponseMap::iterator iter = response_map_.find(url.as_string());
  CHECK(iter != response_map_.end());
  HttpResponse* http_response = iter->second;
  ResponseHeaders* response = http_response->mutable_header();
  response->Add(name, value);
  response->ComputeCaching();
}

void MockUrlFetcher::SetConditionalResponse(
    const StringPiece& url, int64 last_modified_time,
    const ResponseHeaders& response_header, const StringPiece& response_body) {
  GoogleString url_string = url.as_string();
  // Delete any old response.
  ResponseMap::iterator iter = response_map_.find(url_string);
  if (iter != response_map_.end()) {
    delete iter->second;
    response_map_.erase(iter);
  }

  // Add new response.
  HttpResponse* response = new HttpResponse(last_modified_time,
                                            response_header, response_body);
  response_map_.insert(ResponseMap::value_type(url_string, response));
}

void MockUrlFetcher::Clear() {
  STLDeleteContainerPairSecondPointers(response_map_.begin(),
                                       response_map_.end());
  response_map_.clear();
}

bool MockUrlFetcher::StreamingFetchUrl(const GoogleString& url,
                                       const RequestHeaders& request_headers,
                                       ResponseHeaders* response_headers,
                                       Writer* response_writer,
                                       MessageHandler* message_handler) {
  bool ret = false;
  if (enabled_) {
    ResponseMap::iterator iter = response_map_.find(url);
    if (iter != response_map_.end()) {
      const HttpResponse* response = iter->second;
      // Check if we should return 304 Not Modified or full response.
      ConstStringStarVector values;
      int64 if_modified_since_time;
      if (request_headers.Lookup(HttpAttributes::kIfModifiedSince, &values) &&
          values.size() == 1 &&
          ConvertStringToTime(*values[0], &if_modified_since_time) &&
          if_modified_since_time >= response->last_modified_time()) {
        // We recieved an If-Modified-Since header with a date that was
        // parsable and at least as new our new resource.
        //
        // So, just serve 304 Not Modified.
        response_headers->SetStatusAndReason(HttpStatus::kNotModified);
        // TODO(sligocki): Perhaps allow other headers to be set.
        // Date is technically required to be set.
      } else {
        // Otherwise serve a normal 200 OK response.
        response_headers->CopyFrom(response->header());
        if (fail_after_headers_) {
          return false;
        }
        if (update_date_headers_) {
          CHECK(timer_ != NULL);
          // Update Date headers.
          response_headers->SetDate(timer_->NowMs());
        }
        response_headers->ComputeCaching();

        if (!(response->body().empty() && omit_empty_writes_)) {
          response_writer->Write(response->body(), message_handler);
        }
      }
      ret = true;
    } else {
      // This is used in tests and we do not expect the test to request a
      // resource that we don't have. So fail if we do.
      //
      // If you want a 404 response, you must explicitly use SetResponse.
      if (fail_on_unexpected_) {
        EXPECT_TRUE(false) << "Requested unset url " << url;
      }
    }
  }
  return ret;
}

}  // namespace net_instaweb
