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

#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;

namespace {

// This class wraps around an external Writer and passes through calls to
// that Writer as long as ->release() has not been called on the
// SyncFetcherAdapterCallback passed to the constructor. See the comments for
// SyncFetcherAdapterCallback::response_headers() and writer() in the header for
// why we need this.
class ProtectedWriter : public Writer {
 public:
  ProtectedWriter(SyncFetcherAdapterCallback* callback, Writer* orig_writer)
      : callback_(callback),
        orig_writer_(orig_writer) {
  }

  virtual bool Write(const StringPiece& buf, MessageHandler* handler) {
    bool ret = true;

    // If the callback has not timed out and been released, then pass
    // the data through.
    if (callback_->LockIfNotReleased()) {
      ret = orig_writer_->Write(buf, handler);
      callback_->Unlock();
    }
    return ret;
  }

  virtual bool Flush(MessageHandler* handler) {
    bool ret = true;

    // If the callback has not timed out and been released, then pass
    // the flush through.
    if (callback_->LockIfNotReleased()) {
      ret = orig_writer_->Flush(handler);
      callback_->Unlock();
    }
    return ret;
  }

 private:
  SyncFetcherAdapterCallback* callback_;
  Writer* orig_writer_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedWriter);
};

}  // namespace

SyncFetcherAdapterCallback::SyncFetcherAdapterCallback(
    ThreadSystem* thread_system,  ResponseHeaders* response_headers,
    Writer* writer)
    : mutex_(thread_system->NewMutex()),
      done_(false),
      success_(false),
      released_(false),
      response_headers_(response_headers),
      writer_(new ProtectedWriter(this, writer)) {
}

SyncFetcherAdapterCallback::~SyncFetcherAdapterCallback() {
}

void SyncFetcherAdapterCallback::Done(bool success) {
  mutex_->Lock();
  done_ = true;
  success_ = success;
  if (released_) {
    mutex_->Unlock();
    delete this;
  } else {
    response_headers_->CopyFrom(response_headers_buffer_);
    mutex_->Unlock();
  }
}

void SyncFetcherAdapterCallback::Release() {
  mutex_->Lock();
  released_ = true;
  if (done_) {
    mutex_->Unlock();
    delete this;
  } else {
    mutex_->Unlock();
  }
}

bool SyncFetcherAdapterCallback::done() const {
  ScopedMutex hold_lock(mutex_.get());
  return done_;
}

bool SyncFetcherAdapterCallback::success() const {
  ScopedMutex hold_lock(mutex_.get());
  return success_;
}

bool SyncFetcherAdapterCallback::released() const {
  ScopedMutex hold_lock(mutex_.get());
  return released_;
}

bool SyncFetcherAdapterCallback::LockIfNotReleased() {
  mutex_->Lock();
  if (!released_) {
    return true;
  } else {
    mutex_->Unlock();
    return false;
  }
}

void SyncFetcherAdapterCallback::Unlock() {
  mutex_->Unlock();
}

}  // namespace net_instaweb
