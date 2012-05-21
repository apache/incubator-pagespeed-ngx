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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_MOCK_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_MOCK_URL_FETCHER_H_

#include <map>
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class MockTimer;
class RequestHeaders;
class Writer;

// Simple UrlFetcher meant for tests, you can set responses for individual URLs.
// Meant only for testing.
class MockUrlFetcher : public UrlFetcher {
 public:
  MockUrlFetcher() : enabled_(true), fail_on_unexpected_(true),
                     update_date_headers_(false), omit_empty_writes_(false),
                     fail_after_headers_(false), verify_host_header_(false),
                     timer_(NULL) {}
  virtual ~MockUrlFetcher();

  void SetResponse(const StringPiece& url,
                   const ResponseHeaders& response_header,
                   const StringPiece& response_body);

  // Adds a new response-header attribute name/value pair to an existing
  // response.  If the response does not already exist, the method check-fails.
  void AddToResponse(const StringPiece& url,
                     const StringPiece& name,
                     const StringPiece& value);

  // Set a conditional response which will either respond with the supplied
  // response_headers and response_body or a simple 304 Not Modified depending
  // upon last_modified_time and conditional GET "If-Modified-Since" headers.
  void SetConditionalResponse(const StringPiece& url,
                              int64 last_modified_date,
                              const GoogleString& etag,
                              const ResponseHeaders& response_header,
                              const StringPiece& response_body);

  // Fetching unset URLs will cause EXPECT failures as well as return false.
  virtual bool StreamingFetchUrl(const GoogleString& url,
                                 const RequestHeaders& request_headers,
                                 ResponseHeaders* response_headers,
                                 Writer* response_writer,
                                 MessageHandler* message_handler);

  // Indicates that the specified URL should respond with headers and data,
  // but still return a 'false' status.  This is similar to a live fetcher
  // that times out or disconnects while streaming data.
  //
  // This differs from set_fail_after_headers in that it's specific to a
  // URL, and writes the body first before returning failure.
  void SetResponseFailure(const StringPiece& url);

  // Clear all set responses.
  void Clear();

  // Remove a single response. Will be a no-op if no response was set for url.
  void RemoveResponse(const StringPiece& url);

  // When disabled, fetcher will fail (but not crash) for all requests.
  // Use to simulate temporarily not having access to resources, for example.
  void Disable() { enabled_ = false; }
  void Enable() { enabled_ = true; }

  // Set to false if you don't want the fetcher to EXPECT fail on unfound URL.
  // Useful in MockUrlFetcher unittest :)
  void set_fail_on_unexpected(bool x) { fail_on_unexpected_ = x; }

  // Update response header's Date using supplied timer.
  // Note: Must set_timer().
  void set_update_date_headers(bool x) { update_date_headers_ = x; }

  // If set to true (defaults to false) the fetcher will not emit writes of
  // length 0.
  void set_omit_empty_writes(bool x) { omit_empty_writes_ = x; }

  // If set to true (defaults to false) the fetcher will fail after outputting
  // the headers.  See also SetResponseFailure which fails after writing
  // the body.
  void set_fail_after_headers(bool x) { fail_after_headers_ = x; }

  // If set to true (defaults to false) the fetcher will verify that the Host:
  // header is present, and matches the host/port of the requested URL.
  void set_verify_host_header(bool x) { verify_host_header_ = x; }

  void set_timer(MockTimer* timer) { timer_ = timer; }

 private:
  class HttpResponse {
   public:
    HttpResponse(int64 last_modified_time, const GoogleString& etag,
                 const ResponseHeaders& in_header, const StringPiece& in_body)
        : last_modified_time_(last_modified_time),
          etag_(etag),
          body_(in_body.data(), in_body.size()),
          success_(true) {
      header_.CopyFrom(in_header);
    }

    const int64 last_modified_time() const { return last_modified_time_; }
    const GoogleString& etag() const { return etag_; }
    const ResponseHeaders& header() const { return header_; }
    ResponseHeaders* mutable_header() { return &header_; }
    const GoogleString& body() const { return body_; }
    void set_success(bool success) { success_ = success; }
    bool success() const { return success_; }

   private:
    int64 last_modified_time_;
    GoogleString etag_;
    ResponseHeaders header_;
    GoogleString body_;
    bool success_;

    DISALLOW_COPY_AND_ASSIGN(HttpResponse);
  };
  typedef std::map<const GoogleString, HttpResponse*> ResponseMap;

  ResponseMap response_map_;

  bool enabled_;
  bool fail_on_unexpected_;   // Should we EXPECT if unexpected url called?
  bool update_date_headers_;  // Should we update Date headers from timer?
  bool omit_empty_writes_;    // Should we call ->Write with length 0?
  bool fail_after_headers_;   // Should we call Done(false) after headers?
  bool verify_host_header_;   // Should we verify the Host: header?

  MockTimer* timer_;  // Timer to use for updating header dates.

  DISALLOW_COPY_AND_ASSIGN(MockUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_MOCK_URL_FETCHER_H_
