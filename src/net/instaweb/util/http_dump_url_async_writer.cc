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

#include "net/instaweb/util/public/http_dump_url_async_writer.h"

#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class HttpDumpUrlAsyncWriter::Fetch : UrlAsyncFetcher::Callback {
 public:
  Fetch(const std::string& url, const MetaData& request_headers,
        MetaData* response_headers, Writer* response_writer,
        MessageHandler* handler, Callback* callback,
        const std::string& filename, UrlFetcher* dump_fetcher,
        FileSystem* file_system)
      : url_(url), response_headers_(response_headers),
        response_writer_(response_writer), handler_(handler),
        callback_(callback), filename_(filename), dump_fetcher_(dump_fetcher),
        file_system_(file_system), string_writer_(&contents_) {
    request_headers_.CopyFrom(request_headers);
  }

  // Like UrlAsyncFetcher::StreamingFetch, returns true if callback has been
  // called already.
  bool StartFetch(const bool accept_gzip, UrlAsyncFetcher* base_fetcher) {
    // In general we will want to always ask the origin for gzipped output,
    // but we are leaving in variable so this could be overridden by the
    // instantiator of the DumpUrlWriter.
    compress_headers_.CopyFrom(request_headers_);
    if (accept_gzip) {
      compress_headers_.RemoveAll(HttpAttributes::kAcceptEncoding);
      compress_headers_.Add(HttpAttributes::kAcceptEncoding,
                            HttpAttributes::kGzip);
    }

    return base_fetcher->StreamingFetch(url_, compress_headers_,
                                         &compressed_response_, &string_writer_,
                                         handler_, this);
  }

  // Finishes the Fetch when called back.
  void Done(bool success) {
    compressed_response_.RemoveAll(HttpAttributes::kContentLength);
    compressed_response_.Add(HttpAttributes::kContentLength,
                          IntegerToString(contents_.size()).c_str());
    compressed_response_.ComputeCaching();

    // Do not write an empty file if the fetch failed.
    if (success) {
      FileSystem::OutputFile* file = file_system_->OpenTempFile(
          filename_ + ".temp", handler_);
      if (file != NULL) {
        handler_->Message(kInfo, "Storing %s as %s", url_.c_str(),
                          filename_.c_str());
        std::string temp_filename = file->filename();
        FileWriter file_writer(file);
        success = compressed_response_.Write(&file_writer, handler_) &&
            file->Write(contents_, handler_);
        success &= file_system_->Close(file, handler_);
        success &= file_system_->RenameFile(temp_filename.c_str(),
                                        filename_.c_str(),
                                        handler_);
      } else {
        success = false;
      }
    }

    // We are not going to be able to read the response from the file
    // system so we better pass the error message through.
    if (!success) {
      response_headers_->CopyFrom(compressed_response_);
      response_writer_->Write(contents_, handler_);
    } else {
      // Let dump fetcher fetch the actual response so that it can decompress.
      success = dump_fetcher_->StreamingFetchUrl(
          url_, request_headers_, response_headers_, response_writer_,
          handler_);
    }

    callback_->Done(success);
    delete this;
  }

 private:
  const std::string url_;
  SimpleMetaData request_headers_;
  MetaData* response_headers_;
  Writer* response_writer_;
  MessageHandler* handler_;
  Callback* callback_;

  const std::string filename_;
  UrlFetcher* dump_fetcher_;
  FileSystem* file_system_;

  std::string contents_;
  StringWriter string_writer_;
  SimpleMetaData compress_headers_;
  SimpleMetaData compressed_response_;

  DISALLOW_COPY_AND_ASSIGN(Fetch);
};

HttpDumpUrlAsyncWriter::~HttpDumpUrlAsyncWriter() {
}

bool HttpDumpUrlAsyncWriter::StreamingFetch(const std::string& url,
                                            const MetaData& request_headers,
                                            MetaData* response_headers,
                                            Writer* response_writer,
                                            MessageHandler* handler,
                                            Callback* callback) {
  std::string filename;
  dump_fetcher_.GetFilename(GURL(url), &filename, handler);

  if (file_system_->Exists(filename.c_str(), handler).is_true()) {
    bool success = dump_fetcher_.StreamingFetchUrl(
        url, request_headers, response_headers, response_writer, handler);
    callback->Done(success);
    return true;
  } else {
    Fetch* fetch = new Fetch(url, request_headers, response_headers,
                             response_writer, handler, callback, filename,
                             &dump_fetcher_, file_system_);
    return fetch->StartFetch(accept_gzip_, base_fetcher_);
  }
}

}  // namespace net_instaweb
