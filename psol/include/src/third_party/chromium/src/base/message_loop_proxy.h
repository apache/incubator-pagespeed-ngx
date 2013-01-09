// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_PROXY_H_
#define BASE_MESSAGE_LOOP_PROXY_H_
#pragma once

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"

namespace base {

struct MessageLoopProxyTraits;

// This class provides a thread-safe refcounted interface to the Post* methods
// of a message loop. This class can outlive the target message loop. You can
// obtain a MessageLoopProxy via Thread::message_loop_proxy() or
// MessageLoopProxy::CreateForCurrentThread().
class BASE_API MessageLoopProxy
    : public base::RefCountedThreadSafe<MessageLoopProxy,
                                        MessageLoopProxyTraits> {
 public:
  // These methods are the same as in message_loop.h, but are guaranteed to
  // either post the Task to the MessageLoop (if it's still alive), or to
  // delete the Task otherwise.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run; for
  // example the target loop may already be quitting, or in the case of a
  // delayed task a Quit message may preempt it in the message loop queue.
  // Conversely, a return value of false is a guarantee the task will not run.
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        Task* task) = 0;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               Task* task, int64 delay_ms) = 0;
  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   Task* task) = 0;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) = 0;
  // A method which checks if the caller is currently running in the thread that
  // this proxy represents.
  virtual bool BelongsToCurrentThread() = 0;

  template <class T>
  bool DeleteSoon(const tracked_objects::Location& from_here,
                  T* object) {
    return PostNonNestableTask(from_here, new DeleteTask<T>(object));
  }
  template <class T>
  bool ReleaseSoon(const tracked_objects::Location& from_here,
                   T* object) {
    return PostNonNestableTask(from_here, new ReleaseTask<T>(object));
  }

  // Factory method for creating an implementation of MessageLoopProxy
  // for the current thread.
  static scoped_refptr<MessageLoopProxy> CreateForCurrentThread();

 protected:
  friend class RefCountedThreadSafe<MessageLoopProxy, MessageLoopProxyTraits>;
  friend struct MessageLoopProxyTraits;

  MessageLoopProxy();
  virtual ~MessageLoopProxy();

  // Called when the proxy is about to be deleted. Subclasses can override this
  // to provide deletion on specific threads.
  virtual void OnDestruct() const;
};

struct MessageLoopProxyTraits {
  static void Destruct(const MessageLoopProxy* proxy) {
    proxy->OnDestruct();
  }
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_PROXY_H_
