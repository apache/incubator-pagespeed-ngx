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

#ifndef MOD_SPDY_APACHE_FILTERS_HTTP_TO_SPDY_FILTER_H_
#define MOD_SPDY_APACHE_FILTERS_HTTP_TO_SPDY_FILTER_H_

#include <string>

#include "apr_buckets.h"
#include "util_filter.h"

#include "base/basictypes.h"
#include "mod_spdy/common/http_to_spdy_converter.h"

namespace mod_spdy {

class SpdyStream;

// An Apache filter for converting HTTP data into SPDY frames and sending them
// to the output queue of a SpdyStream object.  This is intended to be the
// outermost filter in the output chain of one of our slave connections,
// essentially taking the place of the network socket.
//
// In a previous implementation of this filter, we made this a TRANSCODE-level
// filter rather than a NETWORK-level filter; this had the advantage that we
// could pull HTTP header data directly from the Apache request object, rather
// than having to parse the headers.  However, it had the disadvantage of being
// fragile -- for example, we had an additional output filter whose sole job
// was to deceive Apache into not chunking the response body, and several
// different hooks to try to make sure our output filters stayed in place even
// in the face of Apache's weird error-handling paths.  Also, using a
// NETWORK-level filter decreases the likelihood that we'll break other modules
// that try to use connection-level filters.
class HttpToSpdyFilter {
 public:
  explicit HttpToSpdyFilter(SpdyStream* stream);
  ~HttpToSpdyFilter();

  // Read data from the given brigade and write the result through the given
  // filter. This method is responsible for driving the HTTP to SPDY conversion
  // process.
  apr_status_t Write(ap_filter_t* filter, apr_bucket_brigade* input_brigade);

 private:
  class ReceiverImpl : public HttpToSpdyConverter::SpdyReceiver {
   public:
    explicit ReceiverImpl(SpdyStream* stream);
    virtual ~ReceiverImpl();
    virtual void ReceiveSynReply(net::SpdyHeaderBlock* headers, bool flag_fin);
    virtual void ReceiveData(base::StringPiece data, bool flag_fin);

   private:
    friend class HttpToSpdyFilter;
    SpdyStream* const stream_;

    DISALLOW_COPY_AND_ASSIGN(ReceiverImpl);
  };

  ReceiverImpl receiver_;
  HttpToSpdyConverter converter_;
  bool eos_bucket_received_;

  DISALLOW_COPY_AND_ASSIGN(HttpToSpdyFilter);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_FILTERS_HTTP_TO_SPDY_FILTER_H_
