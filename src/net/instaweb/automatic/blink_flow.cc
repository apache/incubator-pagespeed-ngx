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

// Author: nikhilmadan@google.com (Nikhil Madan)
//
// This class manages the flow of a blink request. In order to flush the layout
// early before we start getting bytes back from the fetcher, we trigger a cache
// lookup for the layout.
// If the layout is found, we flush it out and then trigger the normal
// ProxyFetch flow with customized options which extracts json from the page and
// sends it out.
// If the layout is not found in cache, we pass this request through the normal
// ProxyFetch flow and trigger an asynchronous fetch for the sample page of the
// family to which the current request belongs, create a driver to parse it and
// store the extracted layout in the cache.

#include "net/instaweb/automatic/public/blink_flow.h"

#include <cstddef>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;

const char kLayoutCachePrefix[] = "layout/";
const int kLayoutCachePrefixLength = strlen(kLayoutCachePrefix);
const char kLayoutMarker[] = "<!--GooglePanel **** Layout end ****-->";

namespace {

// AsyncFetch that doesn't call HeadersComplete() on the base fetch. Note that
// this class only links the request headers from the base fetch and does not
// link the response headers.
// This is used as a wrapper around the base fetch when the layout is found in
// cache. This is done because the response headers and the layout have been
// already been flushed out in the base fetch and we don't want to call
// HeadersComplete() twice on the base fetch.
// This class deletes itself when HandleDone() is called.
class AsyncFetchWithHeadersInhibited : public AsyncFetchUsingWriter {
 public:
  explicit AsyncFetchWithHeadersInhibited(AsyncFetch* fetch)
      : AsyncFetchUsingWriter(fetch),
        base_fetch_(fetch) {
    set_request_headers(fetch->request_headers());
  }

 private:
  virtual ~AsyncFetchWithHeadersInhibited() {}
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
    delete this;
  }

  AsyncFetch* base_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithHeadersInhibited);
};

// AsyncFetch that fetches the sample page, initiates a new RewriteDriver to
// compute the layout and stores it in cache.
// TODO(rahulbansal): Buffer the html chunked rather than in one string.
class LayoutFetch : public StringAsyncFetch {
 public:
  LayoutFetch(const GoogleString& key,
              ResourceManager* resource_manager,
              RewriteOptions* options)
      : key_(key),
        resource_manager_(resource_manager),
        options_(options) {}

  virtual ~LayoutFetch() {}

  virtual void HandleDone(bool success) {
    if (success) {
      RewriteDriver* layout_computation_driver =
          resource_manager_->NewCustomRewriteDriver(options_.release());
      // Set deadline to 10s since we want maximum filters to complete.
      // Note that no client is blocked waiting for this request to complete.
      layout_computation_driver->set_rewrite_deadline_ms(
          10 * Timer::kSecondMs);
      layout_computation_driver->SetWriter(&value_);
      layout_computation_driver->set_response_headers_ptr(response_headers());

      layout_computation_driver->StartParse(
          key_.substr(kLayoutCachePrefixLength));
      layout_computation_driver->ParseText(buffer());

      // Clean up.
      layout_computation_driver->FinishParseAsync(MakeFunction(
          this, &LayoutFetch::CompleteFinishParse));
    } else {
      // Do nothing since the fetch failed.
      LOG(INFO) << "Background fetch for layout url " << key_ << " failed.";
      delete this;
    }
  }

  void CompleteFinishParse() {
    HTTPCache* http_cache = resource_manager_->http_cache();
    MessageHandler* message_handler = resource_manager_->message_handler();
    value_.SetHeaders(response_headers());

    VLOG(1) << "Adding layout to cache with key = " << key_ << "\theaders = "
            << response_headers()->ToString();
    http_cache->Put(key_, &value_, message_handler);
    delete this;
  }

 private:
  GoogleString key_;
  ResourceManager* resource_manager_;
  scoped_ptr<RewriteOptions> options_;
  HTTPValue value_;

  DISALLOW_COPY_AND_ASSIGN(LayoutFetch);
};

}  // namespace

class BlinkFlow::LayoutFindCallback : public HTTPCache::Callback {
 public:
  explicit LayoutFindCallback(BlinkFlow* blink_fetch)
      : blink_fetch_(blink_fetch) {}

  virtual ~LayoutFindCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result != HTTPCache::kFound) {
      blink_fetch_->LayoutCacheMiss();
    } else {
      StringPiece contents;
      http_value()->ExtractContents(&contents);
      blink_fetch_->LayoutCacheHit(contents, *response_headers());
    }
    delete this;
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return true;
  }

 private:
  BlinkFlow* blink_fetch_;
  DISALLOW_COPY_AND_ASSIGN(LayoutFindCallback);
};

void BlinkFlow::Start(const GoogleString& url,
                      AsyncFetch* base_fetch,
                      const Layout* layout,
                      RewriteOptions* options,
                      ProxyFetchFactory* factory,
                      ResourceManager* manager) {
  BlinkFlow* flow = new BlinkFlow(url, base_fetch, layout, options, factory,
                                  manager);
  flow->InitiateLayoutLookup();
}

void BlinkFlow::InitiateLayoutLookup() {
  const PublisherConfig* config = options_->panel_config();
  GoogleUrl gurl(url_);
  layout_url_ = StrCat(kLayoutCachePrefix, "http://",
                       config->web_site(),
                       layout_->reference_page_url_path());
  LayoutFindCallback* callback = new LayoutFindCallback(this);
  manager_->http_cache()->Find(layout_url_,
                               manager_->message_handler(),
                               callback);
}

BlinkFlow::~BlinkFlow() {}

void BlinkFlow::LayoutCacheHit(const StringPiece& content,
                               const ResponseHeaders& headers) {
  if (headers.status_code() != HttpStatus::kOK) {
    LayoutCacheMiss();
    return;
  }

  // NOTE: Since we compute layout in background and only get it in serialized
  // form, we have to strip everything after the layout marker.
  size_t pos = content.find(kLayoutMarker);
  if (pos == StringPiece::npos) {
    LOG(DFATAL) << "Layout marker not found: " << content;
    LayoutCacheMiss();
    return;
  }

  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->CopyFrom(headers);
  // Remove any Etag headers from the layout response. Note that an Etag is
  // added by the HTTPCache for all responses that don't already have one.
  response_headers->RemoveAll(HttpAttributes::kEtag);
  base_fetch_->HeadersComplete();

  base_fetch_->Write(content.substr(0, pos), manager_->message_handler());
  base_fetch_->Write("<script src=\"/webinstant/blink.js\"></script>",
                     manager_->message_handler());
  base_fetch_->Write("<script>var panelLoader = new PanelLoader();</script>",
                     manager_->message_handler());
  base_fetch_->Flush(manager_->message_handler());

  options_->EnableFilter(RewriteOptions::kComputePanelJson);
  options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options_->EnableFilter(RewriteOptions::kDisableJavascript);
  options_->EnableFilter(RewriteOptions::kInlineImages);
  TriggerProxyFetch(true);
}

void BlinkFlow::LayoutCacheMiss() {
  ComputeLayoutInBackground();
  TriggerProxyFetch(false);
}

void BlinkFlow::TriggerProxyFetch(bool layout_found) {
  AsyncFetch* fetch = base_fetch_;
  if (layout_found) {
    // Remove any headers that can lead to a 304, since blink can't handle 304s.
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfNoneMatch);
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfModifiedSince);
    // Pass a new fetch into proxy fetch that inhibits HeadersComplete() on the
    // base fetch. It also doesn't attach the response headers from the base
    // fetch since headers have already been flushed out.
    fetch = new AsyncFetchWithHeadersInhibited(base_fetch_);
  }
  factory_->StartNewProxyFetch(url_, fetch, options_);
  delete this;
}

void BlinkFlow::ComputeLayoutInBackground() {
  RewriteOptions* options = options_->Clone();
  options->EnableFilter(RewriteOptions::kComputeLayout);
  options->EnableFilter(RewriteOptions::kDisableJavascript);
  options->EnableFilter(RewriteOptions::kHtmlWriterFilter);

  LayoutFetch* layout_fetch = new LayoutFetch(layout_url_, manager_, options);

  manager_->url_async_fetcher()->Fetch(
      layout_url_.substr(kLayoutCachePrefixLength),
      manager_->message_handler(),
      layout_fetch);
}

}  // namespace net_instaweb
