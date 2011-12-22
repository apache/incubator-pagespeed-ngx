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

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// This Fetch layer helps work with origin servers that serve gzipped
// content even when request-headers do not include
// accept-encoding:gzip.  In that scenario, this class inflates the
// content and strips the content-encoding:gzip response header.
//
// TODO(jmarantz): Note that this filter enables fetchers to unconditionally
// add accept-encoding:gzip to reduce network overhead, and this class can
// then transparently handle the inflating.  Currently the intent is just
// to work better with servers that do not honor the request headers.
class InflatingFetch : public SharedAsyncFetch {
 public:
  explicit InflatingFetch(AsyncFetch* fetch);
  virtual ~InflatingFetch();

 protected:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);
  virtual void Reset();

 private:
  void InitInflater(GzipInflater::InflateType, const StringPiece& value);

  scoped_ptr<GzipInflater> inflater_;
  bool inflate_failure_;
  DISALLOW_COPY_AND_ASSIGN(InflatingFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_INFLATING_FETCH_H_
