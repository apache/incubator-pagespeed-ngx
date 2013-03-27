// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_
#define BASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_

#include "base/base_export.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/object_watcher.h"
#else
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#endif

namespace base {

class Flag;
class AsyncWaiter;
class AsyncCallbackTask;
class WaitableEvent;

// -----------------------------------------------------------------------------
// This class provides a way to wait on a WaitableEvent asynchronously.
//
// Each instance of this object can be waiting on a single WaitableEvent. When
// the waitable event is signaled, a callback is made in the thread of a given
// MessageLoop. This callback can be deleted by deleting the waiter.
//
// Typical usage:
//
//   class MyClass : public base::WaitableEventWatcher::Delegate {
//    public:
//     void DoStuffWhenSignaled(WaitableEvent *waitable_event) {
//       watcher_.StartWatching(waitable_event, this);
//     }
//     virtual void OnWaitableEventSignaled(WaitableEvent* waitable_event) {
//       // OK, time to do stuff!
//     }
//    private:
//     base::WaitableEventWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when waitable_event
// becomes signaled. WaitableEventWatcher makes this task easy. When MyClass
// goes out of scope, the watcher_ will be destroyed, and there is no need to
// worry about OnWaitableEventSignaled being called on a deleted MyClass
// pointer.
//
// BEWARE: With automatically reset WaitableEvents, a signal may be lost if it
// occurs just before a WaitableEventWatcher is deleted. There is currently no
// safe way to stop watching an automatic reset WaitableEvent without possibly
// missing a signal.
//
// NOTE: you /are/ allowed to delete the WaitableEvent while still waiting on
// it with a Watcher. It will act as if the event was never signaled.
// -----------------------------------------------------------------------------

class BASE_EXPORT WaitableEventWatcher
#if !defined(OS_WIN)
    : public MessageLoop::DestructionObserver
#endif
{
 public:

  WaitableEventWatcher();
  virtual ~WaitableEventWatcher();

  class BASE_EXPORT Delegate {
   public:
    // -------------------------------------------------------------------------
    // This is called on the MessageLoop thread when WaitableEvent has been
    // signaled.
    //
    // Note: the event may not be signaled by the time that this function is
    // called. This indicates only that it has been signaled at some point in
    // the past.
    // -------------------------------------------------------------------------
    virtual void OnWaitableEventSignaled(WaitableEvent* waitable_event) = 0;

   protected:
    virtual ~Delegate() { }
  };

  // ---------------------------------------------------------------------------
  // When @event is signaled, the given delegate is called on the thread of the
  // current message loop when StartWatching is called. The delegate is not
  // deleted.
  // ---------------------------------------------------------------------------
  bool StartWatching(WaitableEvent* event, Delegate* delegate);

  // ---------------------------------------------------------------------------
  // Cancel the current watch. Must be called from the same thread which
  // started the watch.
  //
  // Does nothing if no event is being watched, nor if the watch has completed.
  // The delegate will *not* be called for the current watch after this
  // function returns. Since the delegate runs on the same thread as this
  // function, it cannot be called during this function either.
  // ---------------------------------------------------------------------------
  void StopWatching();

  // ---------------------------------------------------------------------------
  // Return the currently watched event, or NULL if no object is currently being
  // watched.
  // ---------------------------------------------------------------------------
  WaitableEvent* GetWatchedEvent();

  // ---------------------------------------------------------------------------
  // Return the delegate, or NULL if there is no delegate.
  // ---------------------------------------------------------------------------
  Delegate* delegate() {
    return delegate_;
  }

 private:
#if defined(OS_WIN)
  // ---------------------------------------------------------------------------
  // The helper class exists because, if WaitableEventWatcher were to inherit
  // from ObjectWatcher::Delegate, then it couldn't also have an inner class
  // called Delegate (at least on Windows). Thus this object exists to proxy
  // the callback function
  // ---------------------------------------------------------------------------
  class ObjectWatcherHelper : public win::ObjectWatcher::Delegate {
   public:
    ObjectWatcherHelper(WaitableEventWatcher* watcher);

    // -------------------------------------------------------------------------
    // Implementation of ObjectWatcher::Delegate
    // -------------------------------------------------------------------------
    void OnObjectSignaled(HANDLE h);

   private:
    WaitableEventWatcher *const watcher_;
  };

  void OnObjectSignaled();

  ObjectWatcherHelper helper_;
  win::ObjectWatcher watcher_;
#else
  // ---------------------------------------------------------------------------
  // Implementation of MessageLoop::DestructionObserver
  // ---------------------------------------------------------------------------
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  MessageLoop* message_loop_;
  scoped_refptr<Flag> cancel_flag_;
  AsyncWaiter* waiter_;
  base::Closure callback_;
  scoped_refptr<WaitableEvent::WaitableEventKernel> kernel_;
#endif

  WaitableEvent* event_;

  Delegate* delegate_;
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_
