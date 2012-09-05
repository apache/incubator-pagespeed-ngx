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

// This class is a simple wrapper around another fetcher that adds headers to
// requests based on settings in the rewrite options before passing them on to
// the backend fetcher.

#ifndef NET_INSTAWEB_APACHE_ADD_HEADERS_FETCHER_H_
#define NET_INSTAWEB_APACHE_ADD_HEADERS_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class RewriteOptions;
class MessageHandler;

class AddHeadersFetcher : public UrlAsyncFetcher {
 public:
  AddHeadersFetcher(const RewriteOptions* options,
                    UrlAsyncFetcher* backend_fetcher);
  virtual ~AddHeadersFetcher();

  virtual bool SupportsHttps() const {
    return backend_fetcher_->SupportsHttps();
  }

  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* callback);

 private:
  const RewriteOptions* const options_;
  UrlAsyncFetcher* const backend_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AddHeadersFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_ADD_HEADERS_FETCHER_H_
