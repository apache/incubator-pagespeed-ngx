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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_HTTP_DUMP_URL_ASYNC_WRITER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_HTTP_DUMP_URL_ASYNC_WRITER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class FileSystem;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Timer;
class Writer;

// HttpDumpWriter checks to see whether the HTTP dump is available on the
// filesystem.  If not, it fetches it from another fetcher (e.g. one that
// uses the network) and writes it to the filesystem so that HttpDumpFetcher
// can find it.
class HttpDumpUrlAsyncWriter : public UrlAsyncFetcher {
 public:
  HttpDumpUrlAsyncWriter(const StringPiece& root_dir,
                         UrlAsyncFetcher* base_fetcher,
                         FileSystem* file_system,
                         Timer* timer)
      : dump_fetcher_(root_dir, file_system, timer),
        base_fetcher_(base_fetcher),
        file_system_(file_system),
        accept_gzip_(true) {
    root_dir.CopyToString(&root_dir_);
  }
  virtual ~HttpDumpUrlAsyncWriter();

  // This is a synchronous/blocking implementation.
  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback);

  // Controls whether we will request and save gzipped content to the
  // file system.  Note that http_dump_url_fetcher will inflate on
  // read if its caller does not want gzipped output.
  void set_accept_gzip(bool x) { accept_gzip_ = x; }

 private:
  // Helper class to manage individual fetchs.
  class Fetch;

  HttpDumpUrlFetcher dump_fetcher_;
  // Used to fetch urls that aren't in the dump yet.
  UrlAsyncFetcher* base_fetcher_;
  GoogleString root_dir_;  // Root directory of the HTTP dumps.
  FileSystem* file_system_;
  bool accept_gzip_;

  DISALLOW_COPY_AND_ASSIGN(HttpDumpUrlAsyncWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_DUMP_URL_ASYNC_WRITER_H_
