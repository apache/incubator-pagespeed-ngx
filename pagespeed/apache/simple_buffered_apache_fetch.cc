// Copyright 2015 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)
// (Based on apache_fetch.cc)

#include "pagespeed/apache/simple_buffered_apache_fetch.h"

#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/thread/scheduler_sequence.h"

namespace net_instaweb {

SimpleBufferedApacheFetch::SimpleBufferedApacheFetch(
    const RequestContextPtr& request_context,
    RequestHeaders* request_headers,
    ThreadSystem* thread_system,
    request_rec* req,
    MessageHandler* handler)
    : AsyncFetch(request_context),
      apache_writer_(new ApacheWriter(req, thread_system)),
      message_handler_(handler),
      mutex_(thread_system->NewMutex()),
      notify_(mutex_->NewCondvar()),
      done_(false),
      wait_called_(false) {
  // We are proxying content, and the caching in the http configuration
  // should not apply; we want to use the caching from the proxy.
  apache_writer_->set_disable_downstream_header_filters(true);
  SetRequestHeadersTakingOwnership(request_headers);
}

SimpleBufferedApacheFetch::~SimpleBufferedApacheFetch() {
  CHECK(wait_called_);
}

// Called on the apache request thread.  Blocks until the request is retired.
void SimpleBufferedApacheFetch::Wait() {
  ScopedMutex lock(mutex_.get());
  CHECK(!wait_called_);
  wait_called_ = true;

  while (!done_) {
    notify_->Wait();
  }
  SendOutHeaders();
  if (!output_bytes_.empty()) {
    apache_writer_->Write(output_bytes_, message_handler_);
  }
}

// Called by other threads.
void SimpleBufferedApacheFetch::HandleHeadersComplete() {
  // Do nothing on this thread right now.  When done waiting we'll deal with
  // headers on the request thread.
}

// Called on the request thread.
void SimpleBufferedApacheFetch::SendOutHeaders() {
  if (content_length_known()) {
    apache_writer_->set_content_length(content_length());
  }
  apache_writer_->OutputHeaders(response_headers());
}

// Called by other threads.
void SimpleBufferedApacheFetch::HandleDone(bool success) {
  {
    ScopedMutex lock(mutex_.get());
    done_ = true;

    // TODO(morlovich): ApacheFetch at least warns when success = false,
    // and headers were successful.  Right now we can do better, but only
    // because we don't stream.
    notify_->Signal();
  }
}

bool SimpleBufferedApacheFetch::HandleWrite(const StringPiece& sp,
                                            MessageHandler* handler) {
  ScopedMutex lock(mutex_.get());
  sp.AppendToString(&output_bytes_);
  return true;
}

bool SimpleBufferedApacheFetch::HandleFlush(MessageHandler* handler) {
  // Don't pass flushes through.... For now.
  return true;
}

bool SimpleBufferedApacheFetch::IsCachedResultValid(
    const ResponseHeaders& headers) {
  LOG(WARNING) << "SimpleBufferedApacheFetch::IsCachedResultValid called; "
                  "should only get this far in tests";
  return true;
}

}  // namespace net_instaweb
