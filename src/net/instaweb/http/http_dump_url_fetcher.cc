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

#include "net/instaweb/http/public/http_dump_url_fetcher.h"

#include <stdio.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/http/public/http_response_parser.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include <string>
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

namespace {

static const char kErrorHtml[] =
    "<html><head><title>HttpDumpUrlFetcher Error</title></head>"
    "<body><h1>HttpDumpUrlFetcher Error</h1></body></html>";

void ApplyTimeDelta(const char* attr, int64 delta_ms,
                    ResponseHeaders* headers) {
  int64 time_ms;
  if (headers->ParseDateHeader(attr, &time_ms) && (time_ms > delta_ms)) {
    headers->UpdateDateHeader(attr, time_ms + delta_ms);
  }
}

// The slurped files we read off the disk will contain a Date header from the
// time we did the slurp.  They may have an Expires header shortly after that.
// As part of the dump-fetching process, we will want to correct the Date
// header based on the current time, and also update the Expires header by
// the same delta.
void CorrectDateHeaders(int64 now_ms, ResponseHeaders* headers) {
  int64 date_ms;
  if (headers->ParseDateHeader(HttpAttributes::kDate, &date_ms) &&
      (date_ms < now_ms)) {
    int64 delta_ms = now_ms - date_ms;
    headers->UpdateDateHeader(HttpAttributes::kDate, now_ms);
    ApplyTimeDelta(HttpAttributes::kExpires, delta_ms, headers);
    ApplyTimeDelta(HttpAttributes::kLastModified, delta_ms, headers);
  }
}

}  // namespace

const char HttpDumpUrlFetcher::kGzipContentLengthAttribute[] =
    "X-Instaweb-Gzip-Content-Length";

HttpDumpUrlFetcher::HttpDumpUrlFetcher(const StringPiece& root_dir,
                                       FileSystem* file_system,
                                       Timer* timer)
    : root_dir_(root_dir.data(), root_dir.size()),
      file_system_(file_system),
      timer_(timer),
      error_body_(kErrorHtml) {
  EnsureEndsInSlash(&root_dir_);
}

HttpDumpUrlFetcher::~HttpDumpUrlFetcher() {
}

bool HttpDumpUrlFetcher::GetFilenameFromUrl(const StringPiece& root_dir,
                                            const GoogleUrl& gurl,
                                            std::string* filename,
                                            MessageHandler* handler) {
  bool ret = false;
  if (!EndsInSlash(root_dir)) {
    handler->Message(kError,
                     "GetFilenameFromUrl: root_dir must end in slash, was %s",
                     root_dir.as_string().c_str());
  } else if (!gurl.is_valid()) {
    handler->Message(kError, "GetFilenameFromUrl: gurl is invalid");
  } else {
    ret = true;

    // Seperate the url into domain and path.  Note: we ignore scheme, username,
    // password, port and ref (stuff after '#').
    // TODO(sligocki): Perhaps we should include these (except ref).
    StringPiece domain = gurl.Host();
    std::string path(gurl.Path().data(), gurl.Path().length());

    // Add other bits of url used by latency lab.
    if (!gurl.Query().empty()) {  // Part after '?' in url.
      path.append(1, '?');
      path.append(gurl.Query().data(), gurl.Query().length());
    }

    FilenameEncoder encoder;
    const std::string prefix = StrCat(root_dir, domain);
    encoder.Encode(prefix, path, filename);  // Writes encoded filename.
  }
  return ret;
}

bool HttpDumpUrlFetcher::GetFilenamePrefixFromUrl(const StringPiece& root_dir,
                                                  const GoogleUrl& url,
                                                  std::string* filename,
                                                  MessageHandler* handler) {
  handler->Check(EndsInSlash(url.Spec()),
                 "Prefix url must end in '/', was %s", url.spec_c_str());
  bool ret = GetFilenameFromUrl(root_dir, url, filename, handler);
  if (ret) {
    size_t last_slash = filename->find_last_of('/');
    CHECK(last_slash != std::string::npos);
    filename->resize(last_slash + 1);
  }
  return ret;
}

void HttpDumpUrlFetcher::RespondError(ResponseHeaders* response_headers,
                                      Writer* response_writer,
                                      MessageHandler* handler) {
  response_headers->SetStatusAndReason(HttpStatus::kNotFound);
  response_headers->Add(HttpAttributes::kContentType, "text/html");
  response_headers->ComputeCaching();
  response_writer->Write(error_body_, handler);
}

// Passes Http contents through to another writer, optionally
// gunzipping if want_gzip is set (and content is gzipped).
class HttpResponseWriter : public Writer {
 public:
  HttpResponseWriter(const StringPiece& url, bool want_gzip, Writer* writer,
                     ResponseHeaders* response)
      : url_(url.data(), url.size()),
        content_length_(0),
        gzip_content_length_(0),
        want_gzip_(want_gzip),
        first_write_(true),
        writer_(writer),
        response_(response) {
  }

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    bool ret = true;

    // We don't store the request headers with the slurped file.  So if
    // we slurp with a gzipped encoding, but the requester wants to see
    // cleartext, then we will convert inline in the Writer.  Determine
    // that the first time Write() is called.
    if (first_write_) {
      first_write_ = false;
      CHECK(response_->headers_complete());
      CharStarVector v;
      if (!want_gzip_ && response_->IsGzipped()) {
        inflater_.reset(new GzipInflater(GzipInflater::kGzip));
        CHECK(inflater_->Init());
        response_->RemoveAll(HttpAttributes::kContentEncoding);
      }
    }
    if (inflater_.get() != NULL) {
      CHECK(!inflater_->HasUnconsumedInput());
      CHECK(inflater_->SetInput(str.data(), str.size()));
      gzip_content_length_ += str.size();
      while (inflater_->HasUnconsumedInput() && ret) {
        char buf[kStackBufferSize];
        int bytes = inflater_->InflateBytes(buf, sizeof(buf));
        if (bytes == 0) {
          handler->Error(url_.c_str(), 0,
                         "zlib reported unconsumed data but yielded 0 bytes");
          ret = false;
        } else {
          if (inflater_->error()) {
            handler->Error(url_.c_str(), 0, "zlib inflate error");
            ret = false;
          } else {
            ret = writer_->Write(StringPiece(buf, bytes), handler);
            content_length_ += bytes;
          }
        }
      }
    } else {
      ret = writer_->Write(str, handler);
      content_length_ += str.size();
    }
    return ret;
  }

  bool Flush(MessageHandler* handler) {
    return writer_->Flush(handler);
  }

  int content_length() const { return content_length_; }
  int gzip_content_length() const { return gzip_content_length_; }

 private:
  std::string url_;
  int content_length_;
  int gzip_content_length_;
  bool want_gzip_;
  bool first_write_;
  Writer* writer_;
  ResponseHeaders* response_;
  scoped_ptr<GzipInflater> inflater_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseWriter);
};

bool HttpDumpUrlFetcher::StreamingFetchUrl(
    const std::string& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, Writer* response_writer,
    MessageHandler* handler) {
  bool ret = false;
  std::string filename;
  GoogleUrl gurl(url);
  if (gurl.is_valid() && gurl.is_standard() &&
      GetFilenameFromUrl(root_dir_, gurl, &filename, handler)) {
    NullMessageHandler null_handler;
    // Pass in NullMessageHandler so that we don't get errors for file not found
    FileSystem::InputFile* file =
        file_system_->OpenInputFile(filename.c_str(), &null_handler);
    if (file != NULL) {
      CharStarVector v;
      // TODO(jmarantz): handle 'deflate'.
      bool want_gzip = request_headers.AcceptsGzip();
      HttpResponseWriter writer(url, want_gzip, response_writer,
                                response_headers);
      HttpResponseParser response(response_headers, &writer, handler);
      if (response.ParseFile(file)) {
        handler->Message(kInfo, "HttpDumpUrlFetcher: Fetched %s as %s",
                         url.c_str(), filename.c_str());
        if (!response.headers_complete()) {
          // Fill in some default headers and body.  Note that if we have
          // a file, then we will return true, even if the file is corrupt.
          RespondError(response_headers, response_writer, handler);
        } else {
          // Update 'date' and 'Expires' headers, if found.
          //
          // TODO(jmarantz): make this conditional based on a flag.
          int64 now_ms = timer_->NowMs();
          CorrectDateHeaders(now_ms, response_headers);
          response_headers->Replace(HttpAttributes::kContentLength,
                                    IntegerToString(writer.content_length()));
        }
        if (writer.gzip_content_length() != 0) {
          response_headers->Add(kGzipContentLengthAttribute, IntegerToString(
              writer.gzip_content_length()).c_str());
        }
        response_headers->ComputeCaching();
        ret = true;
      } else {
        handler->Message(kWarning,
                         "HttpDumpUrlFetcher: Failed to parse %s for %s",
                         filename.c_str(), url.c_str());
      }
      file_system_->Close(file, handler);
    } else {
      handler->Message(kInfo,
                       "HttpDumpUrlFetcher: Failed to find file %s for %s",
                       filename.c_str(), url.c_str());
    }
  } else {
    handler->Message(kError,
                     "HttpDumpUrlFetcher: Requested invalid URL %s",
                     url.c_str());
  }

  if ((urls_.get() != NULL) && urls_->insert(url).second) {
    fprintf(stdout, "url: %s\n", url.c_str());
  }

  return ret;
}

void HttpDumpUrlFetcher::set_print_urls(bool on) {
  if (on) {
    urls_.reset(new StringSet);
  } else {
    urls_.reset(NULL);
  }
}

}  // namespace net_instaweb
