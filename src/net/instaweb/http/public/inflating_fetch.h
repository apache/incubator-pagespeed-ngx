/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_INFLATING_FETCH_H_
#define NET_INSTAWEB_HTTP_PUBLIC_INFLATING_FETCH_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// This Fetch layer helps work with origin servers that serve gzipped
// content even when request-headers do not include
// accept-encoding:gzip.  In that scenario, this class inflates the
// content and strips the content-encoding:gzip response header.
//
// Some servers will serve gzipped content even to clients that didn't
// ask for it.  Depending on the serving environment, we may also want
// to ask backend servers for gzipped content even if we want cleartext
// to be sent to the Write methods.  Users of this class can force this
// by calling EnableGzipFromBackend.
class InflatingFetch : public SharedAsyncFetch {
 public:
  explicit InflatingFetch(AsyncFetch* fetch);
  virtual ~InflatingFetch();

  // Adds accept-encoding:gzip to the request headers sent to the
  // origin.  The data is inflated as we Write it.  If deflate
  // or gzip was already in the request then this has no effect.
  void EnableGzipFromBackend();

 protected:
  // If inflation is required, inflates and passes bytes to the linked fetch,
  // otherwise just passes bytes.
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);

  // Analyzes headers and depending on the request settings and flags will
  // either setup inflater or not.
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);
  virtual void Reset();

 private:
  void InitInflater(GzipInflater::InflateType, const StringPiece& value);

  // If this returns true, it means that we should not inflate incoming data and
  // pass it to the caller as is, since that is what caller requested.
  bool IsCompressionAllowedInRequest();

  scoped_ptr<GzipInflater> inflater_;

  // Caching gate inside IsCompressionAllowedInRequest().
  bool request_checked_for_accept_encoding_;

  // Will be set to true if accepted encoding included gzip and/or deflate.
  bool compression_desired_;

  // Whether any kind of error happened to the inflater. Once set to true, never
  // gets reset.
  bool inflate_failure_;

  DISALLOW_COPY_AND_ASSIGN(InflatingFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_INFLATING_FETCH_H_
