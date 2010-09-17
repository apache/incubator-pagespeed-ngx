// Copyright 2010 Google Inc. All Rights Reserved.
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

// This is temporary implementation for the configurations. This setting will
// not work for windows and/or many other platform.
// TODO(lsong): Use httpd.conf to configure the module.

#include "net/instaweb/apache/html_rewriter_config.h"

#include "net/instaweb/apache/pagespeed_server_context.h"

namespace {

// All these constants are defaults for the conviniece of developing. They are
// sure not working on difference platforms or different configuration of
// systems. Use http.conf to configure those setting.
const int64 kFetcherTimeOut = 30000;  // 30 seconds.
const int64 kResourceFetcherTimeOut = 300000;  // 5 minutes.

}  // namespace


namespace html_rewriter {

const char* GetCachePrefix(PageSpeedServerContext* context) {
  const char* generated_file_prefix = context->config()->generated_file_prefix;
  return generated_file_prefix;
}

const char* GetUrlPrefix(PageSpeedServerContext* context) {
  const char* url_prefix = context->config()->rewrite_url_prefix;
  return url_prefix;
}

const char* GetFileCachePath(PageSpeedServerContext* context) {
  const char* file_cache_path = context->config()->file_cache_path;
  return file_cache_path;
}

const int64 GetFileCacheSize(PageSpeedServerContext* context) {
  return context->config()->file_cache_size_kb;
}

const int64 GetFileCacheCleanInterval(PageSpeedServerContext* context) {
  return context->config()->file_cache_clean_interval_ms;
}

const char* GetFetcherProxy(PageSpeedServerContext* context) {
  const char* fetch_proxy = context->config()->fetch_proxy;
  return fetch_proxy;
}

int64 GetFetcherTimeOut(PageSpeedServerContext* context) {
  int64 time_out = context->config()->fetcher_timeout_ms;
  return time_out <= 0 ? kFetcherTimeOut : time_out;
}

int64 GetResourceFetcherTimeOutMs(PageSpeedServerContext* context) {
  int64 time_out = context->config()->resource_timeout_ms;
  return time_out <= 0 ? kResourceFetcherTimeOut : time_out;
}

}  // namespace html_rewriter
