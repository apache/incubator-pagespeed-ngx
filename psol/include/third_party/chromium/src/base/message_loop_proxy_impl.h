// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_PROXY_IMPL_H_
#define BASE_MESSAGE_LOOP_PROXY_IMPL_H_
#pragma once

#include "base/base_api.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"

namespace base {

// A stock implementation of MessageLoopProxy that takes in a MessageLoop
// and keeps track of its lifetime using the MessageLoop DestructionObserver.
// For now a MessageLoopProxyImpl can only be created for the current thread.
class BASE_API MessageLoopProxyImpl : public MessageLoopProxy,
                                      public MessageLoop::DestructionObserver {
 public:
  virtual ~MessageLoopProxyImpl();

  // MessageLoopProxy implementation
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        Task* task);
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               Task* task, int64 delay_ms);
  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   Task* task);
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms);
  virtual bool BelongsToCurrentThread();

  // MessageLoop::DestructionObserver implementation
  virtual void WillDestroyCurrentMessageLoop();

 protected:
  // Override OnDestruct so that we can delete the object on the target message
  // loop if it still exists.
  virtual void OnDestruct() const;

 private:
  MessageLoopProxyImpl();
  bool PostTaskHelper(const tracked_objects::Location& from_here,
                      Task* task, int64 delay_ms, bool nestable);

  // For the factory method to work
  friend class MessageLoopProxy;

  // The lock that protects access to target_message_loop_.
  mutable base::Lock message_loop_lock_;
  MessageLoop* target_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopProxyImpl);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_PROXY_IMPL_H_

