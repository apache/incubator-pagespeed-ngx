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
//
// Author: jmarantz@google.com (Joshua Marantz)

// Must precede any Apache includes, for now, due a conflict in
// the use of 'OK" as an Instaweb enum and as an Apache #define.
#include "base/string_util.h"
#include "net/instaweb/apache/header_util.h"

// TODO(jmarantz): serf_url_async_fetcher evidently sets
// 'gzip' unconditionally, and the response includes the 'gzip'
// encoding header, but serf unzips the response itself.
//
// I think the correct behavior is that our async fetcher should
// transmit the 'gzip' request header if it was specified in the call
// to StreamingFetch.  This would be easy to fix.
//
// Unfortunately, serf 0.31 appears to unzip the content for us, which
// is not what we are asking for.  And it leaves the 'gzip' response
// header in despite having unzipped it itself.  I have tried later
// versions of serf, but the API is not compatible (easy to deal with)
// but the resulting binary has unresolved symbols.  I am wondering
// whether we will have to go to libcurl.
//
// For now use wget when slurping additional files.

#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_fetcher.h"

// The Apache headers must be after instaweb headers.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "apr_strings.h"  // for apr_pstrdup
#include "http_protocol.h"

namespace net_instaweb {

namespace {

// Default handler when the file is not found
void SlurpDefaultHandler(request_rec* r) {
  ap_set_content_type(r, "text/html; charset=utf-8");
  std::string buf = StringPrintf(
      "<html><head><title>Slurp Error</title></head>"
      "<body><h1>Slurp failed</h1>\n"
      "<p>host=%s\n"
      "<p>uri=%s\n"
      "</body></html>",
      r->hostname, r->unparsed_uri);
  ap_rputs(buf.c_str(), r);
  r->status = HttpStatus::kNotFound;
  r->status_line = "Not Found";
}

// TODO(jmarantz): The ApacheWriter defined below is much more
// efficient than the mechanism we are currently using, which is to
// buffer the entire response in a string and then send it later.
// For some reason, this did not work when I tried it, but it's
// worth another look.

#if 0
class ApacheWriter : public Writer {
 public:
  explicit ApacheWriter(request_rec* r)
      : request_(r),
        size_(0) {
  }

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    ap_rwrite(str.data(), str.size(), request_);
    size_ += str.size();
    return true;
  }

  virtual bool Flush(MessageHandler* handler) {
    return true;
  }

  int size() const { return size_; }

 private:
  request_rec* request_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(ApacheWriter);
};
#endif

}  // namespace

void SlurpUrl(const std::string& uri, ApacheRewriteDriverFactory* factory,
              request_rec* r) {
  SimpleMetaData request_headers, response_headers;
  ApacheHeaderToMetaData(r->headers_in, 0, 0, &request_headers);
  std::string contents;
  StringWriter writer(&contents);

  // TODO(jmarantz) Strip out instaweb pass-through directive,
  // changing "?instaweb=0&" --> "?", "?instaweb=0" --> "", and
  // "&instaweb=0" --> "".

  UrlFetcher* fetcher = factory->ComputeUrlFetcher();
  if (!fetcher->StreamingFetchUrl(uri, request_headers,
                                  &response_headers, &writer,
                                  factory->message_handler())) {
    LOG(ERROR) << "mod_slurp: fetch of url " << uri
               << " failed.\nRequest Headers: " << request_headers.ToString()
               << "\n\nResponse Headers: " << response_headers.ToString();
    SlurpDefaultHandler(r);
    return;
  }

  // Apache2 defaults to set the status line as HTTP/1.1.  If the
  // original content was HTTP/1.0, we need to force the server to use
  // HTTP/1.0.  I'm not sure why/whether we need to do this; it was in
  // mod_static from the sdpy project, which is where I copied this
  // code from.
  if ((response_headers.major_version() == 1) &&
      (response_headers.minor_version() == 0)) {
    apr_table_set(r->subprocess_env, "force-response-1.0", "1");
  }

  char* content_type = NULL;
  CharStarVector v;
  if (response_headers.Lookup("content-type", &v)) {
    CHECK(!v.empty());
    // ap_set_content_type does not make a copy of the string, we need
    // to duplicate it.  Note that we will update the content type below,
    // after transforming the headers.
    content_type = apr_pstrdup(r->pool, v[v.size() - 1]);
    response_headers.RemoveAll("content-type");
  }
  response_headers.RemoveAll("transfer-encoding");

  // TODO(jmarantz): centralize standard header names; probably as
  // static const member variables in util/public/meta_data.h.
  response_headers.RemoveAll("content-length");  // we will recompute
  MetaDataToApacheHeader(response_headers, r->headers_out,
                         &r->status, &r->proto_num);
  LOG(INFO) << "slurp output headers: " << response_headers.ToString();
  if (content_type != NULL) {
    ap_set_content_type(r, content_type);
  }

  // Recompute the content-length, because the content is decoded.
  ap_set_content_length(r, contents.size());
  ap_rwrite(contents.c_str(), contents.size(), r);
}

}  // namespace net_instaweb
