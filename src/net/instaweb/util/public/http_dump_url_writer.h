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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HTTP_DUMP_URL_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HTTP_DUMP_URL_WRITER_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/http_dump_url_fetcher.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

class FileSystem;

// HttpDumpWriter checks to see whether the HTTP dump is available on the
// filesystem.  If not, it fetches it from another fetcher (e.g. one that
// uses the network) and writes it to the filesystem so that HttpDumpFetcher
// can find it.
class HttpDumpUrlWriter : public UrlFetcher {
 public:
  HttpDumpUrlWriter(const StringPiece& root_dir, UrlFetcher* base_fetcher,
                    FileSystem* file_system, Timer* timer)
      : dump_fetcher_(root_dir, file_system, timer),
        base_fetcher_(base_fetcher),
        file_system_(file_system),
        accept_gzip_(true) {
    root_dir.CopyToString(&root_dir_);
  }
  virtual ~HttpDumpUrlWriter();

  // This is a synchronous/blocking implementation.
  virtual bool StreamingFetchUrl(const std::string& url,
                                 const MetaData& request_headers,
                                 MetaData* response_headers,
                                 Writer* response_writer,
                                 MessageHandler* message_handler);

  // Controls whether we will request and save gzipped content to the
  // file system.  Note that http_dump_url_fetcher will inflate on
  // read if its caller does not want gzipped output.
  void set_accept_gzip(bool x) { accept_gzip_ = x; }

  // Print URLs each time they are fetched.
  void set_print_urls(bool on) { dump_fetcher_.set_print_urls(on); }

 private:
  HttpDumpUrlFetcher dump_fetcher_;
  UrlFetcher* base_fetcher_;  // Used to fetch urls that aren't in the dump yet.
  std::string root_dir_;  // Root directory of the HTTP dumps.
  FileSystem* file_system_;
  bool accept_gzip_;

  DISALLOW_COPY_AND_ASSIGN(HttpDumpUrlWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HTTP_DUMP_URL_WRITER_H_
