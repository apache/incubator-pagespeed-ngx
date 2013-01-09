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

#ifndef MOD_SPDY_COMMON_HTTP_STRING_BUILDER_H_
#define MOD_SPDY_COMMON_HTTP_STRING_BUILDER_H_

#include <string>

#include "base/basictypes.h"
#include "mod_spdy/common/http_request_visitor_interface.h"

namespace mod_spdy {

// An HttpRequestVisitorInterface class that appends to a std::string.
class HttpStringBuilder : public HttpRequestVisitorInterface {
 public:
  explicit HttpStringBuilder(std::string* str);
  virtual ~HttpStringBuilder();

  bool is_complete() const { return state_ == COMPLETE; }

  // HttpRequestVisitorInterface methods:
  virtual void OnRequestLine(const base::StringPiece& method,
                             const base::StringPiece& path,
                             const base::StringPiece& version);
  virtual void OnLeadingHeader(const base::StringPiece& key,
                            const base::StringPiece& value);
  virtual void OnLeadingHeadersComplete();
  virtual void OnRawData(const base::StringPiece& data);
  virtual void OnDataChunk(const base::StringPiece& data);
  virtual void OnDataChunksComplete();
  virtual void OnTrailingHeader(const base::StringPiece& key,
                                const base::StringPiece& value);
  virtual void OnTrailingHeadersComplete();
  virtual void OnComplete();

 private:
  enum State {
    REQUEST_LINE,
    LEADING_HEADERS,
    LEADING_HEADERS_COMPLETE,
    RAW_DATA,
    DATA_CHUNKS,
    DATA_CHUNKS_COMPLETE,
    TRAILING_HEADERS,
    TRAILING_HEADERS_COMPLETE,
    COMPLETE
  };

  std::string* const string_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(HttpStringBuilder);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_HTTP_STRING_BUILDER_H_
