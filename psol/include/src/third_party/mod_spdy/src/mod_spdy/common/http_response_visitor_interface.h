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

#ifndef MOD_SPDY_COMMON_HTTP_RESPONSE_VISITOR_INTERFACE_H_
#define MOD_SPDY_COMMON_HTTP_RESPONSE_VISITOR_INTERFACE_H_

#include "base/basictypes.h"
#include "base/string_piece.h"

namespace mod_spdy {

// Interface that gets called back as an HTTP response is visited.
class HttpResponseVisitorInterface {
 public:
  HttpResponseVisitorInterface();
  virtual ~HttpResponseVisitorInterface();

  // Called when an HTTP response status line is visited.  Indicates that a new
  // HTTP response is being visited.
  virtual void OnStatusLine(const base::StringPiece& version,
                            const base::StringPiece& status_code,
                            const base::StringPiece& status_phrase) = 0;

  // Called zero or more times, once for each leading (i.e. normal, not
  // trailing) HTTP header.  This is called after OnStatusLine but before
  // OnLeadingHeadersComplete.
  virtual void OnLeadingHeader(const base::StringPiece& key,
                               const base::StringPiece& value) = 0;

  // Called after the leading HTTP headers have been visited.  This will be
  // called exactly once when the leading headers are done (even if there were
  // no leading headers).  If the `fin` argument is true, the response is now
  // complete (i.e. it has no body) and no more methods will be called.
  virtual void OnLeadingHeadersComplete(bool fin) = 0;

  // Called zero or more times, after OnLeadingHeadersComplete.  If the `fin`
  // argument is true, the response is now complete and no more methods will be
  // called.
  virtual void OnData(const base::StringPiece& data, bool fin) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpResponseVisitorInterface);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_HTTP_RESPONSE_VISITOR_INTERFACE_H_
