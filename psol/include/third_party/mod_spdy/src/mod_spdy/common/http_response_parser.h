// Copyright 2011 Google Inc.
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

#ifndef MOD_SPDY_COMMON_HTTP_RESPONSE_PARSER_H_
#define MOD_SPDY_COMMON_HTTP_RESPONSE_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "base/string_piece.h"

namespace mod_spdy {

class HttpResponseVisitorInterface;

// Parses incoming HTTP response data.  Data is fed in piece by piece with the
// ProcessInput method, and appropriate methods are called on the visitor.
// There is no need to indicate the end of the input, as this is inferred from
// the Content-Length or Transfer-Encoding headers.  If the response uses
// chunked encoding, the parser will de-chunk it.  Note that all data after the
// end of the response body, including trailing headers, will be completely
// ignored.
class HttpResponseParser {
 public:
  explicit HttpResponseParser(HttpResponseVisitorInterface* visitor);
  ~HttpResponseParser();

  // Return true on success, false on failure.
  bool ProcessInput(const base::StringPiece& input_data);
  bool ProcessInput(const char* data, size_t size) {
    return ProcessInput(base::StringPiece(data, size));
  }

  // For unit testing only: Get the remaining number of bytes expected (in the
  // whole response, if we used Content-Length, or just in the current chunk,
  // if we used Transfer-Encoding: chunked).
  uint64 GetRemainingBytesForTest() const { return remaining_bytes_; }

 private:
  enum ParserState {
    STATUS_LINE,
    LEADING_HEADERS,
    LEADING_HEADERS_CHECK_NEXT_LINE,
    CHUNK_START,
    BODY_DATA,
    CHUNK_ENDING,
    COMPLETE
  };

  enum BodyType {
    NO_BODY,
    UNCHUNKED_BODY,
    CHUNKED_BODY
  };

  bool ProcessStatusLine(base::StringPiece* data);
  bool CheckStartOfHeaderLine(const base::StringPiece& data);
  bool ProcessLeadingHeaders(base::StringPiece* data);
  bool ProcessChunkStart(base::StringPiece* data);
  bool ProcessBodyData(base::StringPiece* data);
  bool ProcessChunkEnding(base::StringPiece* data);

  bool ParseStatusLine(const base::StringPiece& text);
  bool ParseLeadingHeader(const base::StringPiece& text);
  bool ParseChunkStart(const base::StringPiece& text);

  HttpResponseVisitorInterface* const visitor_;
  ParserState state_;
  BodyType body_type_;
  uint64 remaining_bytes_;
  std::string buffer_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseParser);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_HTTP_RESPONSE_PARSER_H_
