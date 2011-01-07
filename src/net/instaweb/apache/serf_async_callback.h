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

// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#ifndef NET_INSTAWEB_APACHE_SERF_ASYNC_CALLBACK_H_
#define NET_INSTAWEB_APACHE_SERF_ASYNC_CALLBACK_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

// Class to help run an asynchronous fetch synchronously with a timeout.
class SerfAsyncCallback : public UrlAsyncFetcher::Callback {
 public:
  SerfAsyncCallback(ResponseHeaders* response_headers, Writer* writer);
  virtual ~SerfAsyncCallback();
  virtual void Done(bool success);

  // When implementing a synchronous fetch with a timeout based on an
  // underlying asynchronous mechanism, we need to ensure that we don't
  // write to freed memory if the Done callback fires after the timeout.
  //
  // So we need to make sure the Writer and Response Buffers are owned
  // by this Callback class, which will forward the output and headers
  // to the caller *if* it has not been released by the time the callback
  // is called.
  ResponseHeaders* response_headers() { return &response_headers_buffer_; }
  Writer* writer() { return writer_.get(); }

  // When the 'owner' of this callback -- the code that calls 'new' --
  // is done with it, it can call release.  This will only delete the
  // callback if Done() has been called.  Otherwise it will stay around
  // waiting for Done() to be called, and only then will it be deleted.
  //
  // When Release is called prior to Done(), the writer and response_headers
  // will beo NULLed out in this structure so they will not be updated when
  // Done() is finally called.
  void Release();

  bool done() const { return done_; }
  bool success() const { return success_; }
  bool released() const { return released_; }

 private:
  bool done_;
  bool success_;
  bool released_;
  ResponseHeaders response_headers_buffer_;
  ResponseHeaders* response_headers_;
  scoped_ptr<Writer> writer_;

  DISALLOW_COPY_AND_ASSIGN(SerfAsyncCallback);
};

}  // namespace

#endif  // NET_INSTAWEB_APACHE_SERF_ASYNC_CALLBACK_H_
