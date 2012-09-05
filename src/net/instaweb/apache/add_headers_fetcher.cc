// Copyright 2012 Google Inc.
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
//
// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/apache/add_headers_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/http/public/request_headers.h"

namespace net_instaweb {

AddHeadersFetcher::AddHeadersFetcher(const RewriteOptions* options,
                                     UrlAsyncFetcher* backend_fetcher)
    : options_(options), backend_fetcher_(backend_fetcher) {
}

AddHeadersFetcher::~AddHeadersFetcher() {}

bool AddHeadersFetcher::Fetch(const GoogleString& original_url,
                              MessageHandler* message_handler,
                              AsyncFetch* fetch) {
  RequestHeaders* request_headers = fetch->request_headers();
  for (int i = 0, n = options_->num_custom_fetch_headers(); i < n; ++i) {
    const RewriteOptions::NameValue* nv = options_->custom_fetch_header(i);
    request_headers->Replace(nv->name, nv->value);
  }
  return backend_fetcher_->Fetch(original_url, message_handler, fetch);
}

}  // namespace net_instaweb
