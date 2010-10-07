// Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_APACHE_SERF_URL_FETCHER_H_
#define NET_INSTAWEB_APACHE_SERF_URL_FETCHER_H_

#include <string>
#include "base/basictypes.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

class SerfUrlFetcher : public UrlFetcher {
 public:
  SerfUrlFetcher(int64 fetcher_timeout_ms,
                 SerfUrlAsyncFetcher* async_fetcher);
  virtual ~SerfUrlFetcher();
  virtual bool StreamingFetchUrl(const std::string& url,
                                 const MetaData& request_headers,
                                 MetaData* response_headers,
                                 Writer* fetched_content_writer,
                                 MessageHandler* message_handler);

 private:
  int64 fetcher_timeout_ms_;
  SerfUrlAsyncFetcher* async_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SerfUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_SERF_URL_FETCHER_H_
