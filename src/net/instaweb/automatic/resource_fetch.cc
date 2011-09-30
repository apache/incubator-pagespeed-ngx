/*
 * Copyright 2011 Google Inc.
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

#include "net/instaweb/automatic/public/resource_fetch.h"

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// TODO(sligocki): Stick this in rewrite_driver.h?
class DriverFetcher : public UrlAsyncFetcher {
 public:
  DriverFetcher(RewriteDriver* driver) : driver_(driver) {}
  virtual ~DriverFetcher() {}

  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback) {
    if (!driver_->FetchResource(url, request_headers, response_headers,
                                response_writer, callback)) {
      // FetchResource does not call callback if it returns false. (false
      // means that the resource was not the right format to be a pagespeed
      // resource.)
      callback->Done(false);
    }
    return false;
  }

 private:
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(DriverFetcher);
};

}  // namespace

void ResourceFetch::Start(ResourceManager* manager,
                          const GoogleUrl& url,
                          const RequestHeaders& request_headers,
                          ResponseHeaders* response_headers,
                          Writer* response_writer,
                          UrlAsyncFetcher::Callback* callback) {
  RewriteDriver* driver = manager->NewRewriteDriver();
  LOG(INFO) << "Fetch with RewriteDriver " << driver;
  DriverFetcher driver_fetcher(driver);
  ResourceFetch* resource_fetch = new ResourceFetch(
      url, request_headers, response_headers, response_writer,
      manager->message_handler(), driver, manager->url_async_fetcher(),
      manager->timer(), callback);
  // TODO(sligocki): This will currently fail us on all non-pagespeed
  // resource requests. We should move the check somewhere else.
  driver_fetcher.Fetch(url.Spec().as_string(), request_headers,
                       resource_fetch->response_headers_,
                       manager->message_handler(), resource_fetch);
}

ResourceFetch::ResourceFetch(const GoogleUrl& url,
                             const RequestHeaders& request_headers,
                             ResponseHeaders* response_headers,
                             Writer* response_writer,
                             MessageHandler* handler,
                             RewriteDriver* driver,
                             UrlAsyncFetcher* fetcher,
                             Timer* timer,
                             UrlAsyncFetcher::Callback* callback)
    : response_headers_(response_headers),
      response_writer_(response_writer),
      fetcher_(fetcher),
      message_handler_(handler),
      driver_(driver),
      timer_(timer),
      callback_(callback),
      start_time_us_(timer->NowUs()),
      redirect_count_(0) {
  resource_url_.Reset(url);
  request_headers_.CopyFrom(request_headers);
}

ResourceFetch::~ResourceFetch() {
}

void ResourceFetch::HeadersComplete() {
  // We do not want any cookies (or other person information) in pagespeed
  // resources. They shouldn't be here anyway, but we assure that.
  ConstStringStarVector v;
  DCHECK(!response_headers_->Lookup(HttpAttributes::kSetCookie, &v));
  DCHECK(!response_headers_->Lookup(HttpAttributes::kSetCookie2, &v));
  response_headers_->RemoveAll(HttpAttributes::kSetCookie);
  response_headers_->RemoveAll(HttpAttributes::kSetCookie2);

  // "Vary: Accept-Encoding" for all resources that are transmitted compressed.
  // Server ought to set these, I suppose.
  //response_headers_->Add(HttpAttributes::kVary, "Accept-Encoding");
}

bool ResourceFetch::Write(const StringPiece& content, MessageHandler* handler) {
  return response_writer_->Write(content, handler);
}

bool ResourceFetch::Flush(MessageHandler* handler) {
  return response_writer_->Flush(handler);
}

void ResourceFetch::Done(bool success) {
  if (success) {
    LOG(INFO) << "Resource " << resource_url_.Spec()
              << " : " << response_headers_->status_code() ;
  } else {
    // This is a fetcher failure, like connection refused, not just an error
    // status code.
    LOG(ERROR) << "Fetch failed for resource url " << resource_url_.Spec();
    LOG(WARNING) << "Failed resource " << resource_url_.Spec()
                 << "\nrequest headers:\n" << request_headers_.ToString();

    response_headers_->SetStatusAndReason(HttpStatus::kNotFound);
  }
  RewriteStats* stats = driver_->resource_manager()->rewrite_stats();
  stats->fetch_latency_histogram()->Add(
      (timer_->NowUs() - start_time_us_) / 1000.0);
  stats->total_fetch_count()->IncBy(1);
  driver_->Cleanup();
  callback_->Done(success);
  delete this;
}

}  // namespace net_instaweb
