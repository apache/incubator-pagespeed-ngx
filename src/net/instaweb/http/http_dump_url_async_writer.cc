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

#include "net/instaweb/http/public/http_dump_url_async_writer.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HttpDumpUrlAsyncWriter::DumpFetch : public StringAsyncFetch {
 public:
  DumpFetch(const GoogleString& url, MessageHandler* handler,
            AsyncFetch* base_fetch, const GoogleString& filename,
            UrlFetcher* dump_fetcher, FileSystem* file_system)
      : url_(url), handler_(handler), base_fetch_(base_fetch),
        filename_(filename), dump_fetcher_(dump_fetcher),
        file_system_(file_system) {
  }

  void StartFetch(const bool accept_gzip, UrlAsyncFetcher* base_fetcher) {
    // In general we will want to always ask the origin for gzipped output,
    // but we are leaving in variable so this could be overridden by the
    // instantiator of the DumpUrlWriter.
    request_headers()->CopyFrom(*base_fetch_->request_headers());
    if (accept_gzip) {
      request_headers()->Replace(HttpAttributes::kAcceptEncoding,
                                 HttpAttributes::kGzip);
    }

    base_fetcher->Fetch(url_, handler_, this);
  }

  // Finishes the Fetch when called back.
  virtual void HandleDone(bool success) {
    response_headers()->Replace(HttpAttributes::kContentLength,
                                IntegerToString(buffer().size()));
    response_headers()->ComputeCaching();

    // Do not write an empty file if the fetch failed.
    if (success) {
      FileSystem::OutputFile* file = file_system_->OpenTempFile(
          filename_ + ".temp", handler_);
      if (file != NULL) {
        handler_->Message(kInfo, "Storing %s as %s", url_.c_str(),
                          filename_.c_str());
        GoogleString temp_filename = file->filename();
        FileWriter file_writer(file);
        success = response_headers()->WriteAsHttp(&file_writer, handler_) &&
            file->Write(buffer(), handler_);
        success &= file_system_->Close(file, handler_);
        success &= file_system_->RenameFile(temp_filename.c_str(),
                                            filename_.c_str(),
                                            handler_);
      } else {
        success = false;
      }
    }

    if (success) {
      // Let dump fetcher fetch the actual response so that it can decompress.
      success = dump_fetcher_->StreamingFetchUrl(
          url_, *base_fetch_->request_headers(),
          base_fetch_->response_headers(), base_fetch_, handler_);
    } else if (response_headers()->status_code() != 0) {
      // We are not going to be able to read the response from the file
      // system so we better pass the error message through.
      //
      // Status code == 0 means that the headers were not even parsed, this
      // will cause a DCHECK failure in AsyncFetch, so we don't pass anything
      // through.
      base_fetch_->response_headers()->CopyFrom(*response_headers());
      base_fetch_->HeadersComplete();
      base_fetch_->Write(buffer(), handler_);
    }

    base_fetch_->Done(success);
    delete this;
  }

 private:
  const GoogleString url_;
  MessageHandler* handler_;
  AsyncFetch* base_fetch_;

  const GoogleString filename_;
  UrlFetcher* dump_fetcher_;
  FileSystem* file_system_;

  DISALLOW_COPY_AND_ASSIGN(DumpFetch);
};

HttpDumpUrlAsyncWriter::~HttpDumpUrlAsyncWriter() {
}

void HttpDumpUrlAsyncWriter::Fetch(const GoogleString& url,
                                   MessageHandler* handler,
                                   AsyncFetch* base_fetch) {
  GoogleString filename;
  GoogleUrl gurl(url);
  dump_fetcher_.GetFilename(gurl, &filename, handler);

  if (file_system_->Exists(filename.c_str(), handler).is_true()) {
    bool success = dump_fetcher_.StreamingFetchUrl(
        url, *base_fetch->request_headers(), base_fetch->response_headers(),
        base_fetch, handler);
    base_fetch->Done(success);
  } else {
    DumpFetch* fetch = new DumpFetch(url, handler, base_fetch, filename,
                                     &dump_fetcher_, file_system_);
    fetch->StartFetch(accept_gzip_, base_fetcher_);
  }
}

}  // namespace net_instaweb
