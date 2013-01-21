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

#ifndef MOD_SPDY_COMMON_HTTP_REQUEST_VISITOR_INTERFACE_H_
#define MOD_SPDY_COMMON_HTTP_REQUEST_VISITOR_INTERFACE_H_

#include "base/basictypes.h"
#include "base/string_piece.h"

namespace mod_spdy {

// Interface that gets called back as an HTTP stream is visited.
class HttpRequestVisitorInterface {
 public:
  HttpRequestVisitorInterface();
  virtual ~HttpRequestVisitorInterface();

  // Called when an HTTP request line is visited. Indicates that a new HTTP
  // request is being visited.
  virtual void OnRequestLine(const base::StringPiece& method,
                             const base::StringPiece& path,
                             const base::StringPiece& version) = 0;

  // Called zero or more times, once for each leading (i.e. normal, not
  // trailing) HTTP header.  This is called after OnRequestLine but before
  // OnLeadingHeadersComplete.
  virtual void OnLeadingHeader(const base::StringPiece& key,
                               const base::StringPiece& value) = 0;

  // Called after the leading HTTP headers have been visited.  This will be
  // called exactly once when the leading headers are done (even if there were
  // no leading headers).
  virtual void OnLeadingHeadersComplete() = 0;

  // Called zero or more times, after OnLeadingHeadersComplete.  This method is
  // mutually exclusive with OnDataChunk and OnDataChunksComplete; either data
  // will be raw or chunked, but not both.  If raw data is used, there cannot
  // be trailing headers; the raw data section will be terminated by the call
  // to OnComplete.
  virtual void OnRawData(const base::StringPiece& data) = 0;

  // Called zero or more times, after OnLeadingHeadersComplete, once for each
  // "chunk" of the HTTP body.  This method is mutually exclusive with
  // OnRawData; either data will be raw or chunked, but not both.
  virtual void OnDataChunk(const base::StringPiece& data) = 0;

  // Called when there will be no more data chunks.  There may still be
  // trailing headers, however.  This method is mutually exclusive with
  // OnRawData; either data will be raw or chunked, but not both.
  virtual void OnDataChunksComplete() = 0;

  // Called zero or more times, once for each trailing header.  This is called
  // after OnDataChunksComplete but before OnTrailingHeadersComplete.  It
  // cannot be called if OnRawData was used.
  virtual void OnTrailingHeader(const base::StringPiece& key,
                                const base::StringPiece& value) = 0;

  // Called after all the trailing HTTP headers have been visited.  If there
  // were any trailing headers, this will definitely be called; if there were
  // no trailing headers, it is optional.
  virtual void OnTrailingHeadersComplete() = 0;

  // Called once when the HTTP request is totally done.  This is called
  // immediately after one of OnLeadingHeadersComplete, OnRawData,
  // OnDataChunksComplete, or OnTrailingHeadersComplete.  After this, no more
  // methods will be called.
  virtual void OnComplete() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpRequestVisitorInterface);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_HTTP_REQUEST_VISITOR_INTERFACE_H_
