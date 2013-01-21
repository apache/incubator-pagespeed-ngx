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

#ifndef MOD_SPDY_COMMON_HTTP_TO_SPDY_CONVERTER_H_
#define MOD_SPDY_COMMON_HTTP_TO_SPDY_CONVERTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "mod_spdy/common/http_response_parser.h"
#include "net/spdy/spdy_framer.h"  // for SpdyHeaderBlock

namespace mod_spdy {

// Parses incoming HTTP response data and converts it into equivalent SPDY
// frame data.
class HttpToSpdyConverter {
 public:
  // Interface for the class that will receive frame data from the converter.
  class SpdyReceiver {
   public:
    SpdyReceiver();
    virtual ~SpdyReceiver();

    // Receive a SYN_REPLY frame with the given headers.  The callee is free to
    // mutate the headers map (e.g. to add an extra header) before forwarding
    // it on, but the pointer will not remain valid after this method returns.
    virtual void ReceiveSynReply(net::SpdyHeaderBlock* headers,
                                 bool flag_fin) = 0;

    // Receive a DATA frame with the given payload.  The data pointer will not
    // remain valid after this method returns.
    virtual void ReceiveData(base::StringPiece data, bool flag_fin) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(SpdyReceiver);
  };

  // Create a converter that will send frame data to the given receiver.  The
  // converter does *not* gain ownership of the receiver.
  HttpToSpdyConverter(int spdy_version, SpdyReceiver* receiver);
  ~HttpToSpdyConverter();

  // Parse and process the next chunk of input; return true on success, false
  // on failure.
  bool ProcessInput(base::StringPiece input_data);
  bool ProcessInput(const char* data, size_t size) {
    return ProcessInput(base::StringPiece(data, size));
  }

  // Flush out any buffered data.
  void Flush();

 private:
  class ConverterImpl;
  scoped_ptr<ConverterImpl> impl_;
  HttpResponseParser parser_;

  DISALLOW_COPY_AND_ASSIGN(HttpToSpdyConverter);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_HTTP_TO_SPDY_CONVERTER_H_
