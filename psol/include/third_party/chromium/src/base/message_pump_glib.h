// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_H_
#define BASE_MESSAGE_PUMP_GLIB_H_

#include "base/base_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_pump.h"
#include "base/observer_list.h"
#include "base/time.h"

typedef struct _GMainContext GMainContext;
typedef struct _GPollFD GPollFD;
typedef struct _GSource GSource;

namespace base {

// MessagePumpObserver is notified prior to an event being dispatched. As
// Observers are notified of every change, they have to be FAST! The platform
// specific implementation of the class is in message_pump_gtk/message_pump_x.
class MessagePumpObserver;

// MessagePumpDispatcher is used during a nested invocation of Run to dispatch
// events. If Run is invoked with a non-NULL MessagePumpDispatcher, MessageLoop
// does not dispatch events (or invoke gtk_main_do_event), rather every event is
// passed to Dispatcher's Dispatch method for dispatch. It is up to the
// Dispatcher to dispatch, or not, the event. The platform specific
// implementation of the class is in message_pump_gtk/message_pump_x.
class MessagePumpDispatcher;

// This class implements a base MessagePump needed for TYPE_UI MessageLoops on
// platforms using GLib.
class BASE_EXPORT MessagePumpGlib : public MessagePump {
 public:
  MessagePumpGlib();

  // Like MessagePump::Run, but events are routed through dispatcher.
  virtual void RunWithDispatcher(Delegate* delegate,
                                 MessagePumpDispatcher* dispatcher);

  // Internal methods used for processing the pump callbacks.  They are
  // public for simplicity but should not be used directly.  HandlePrepare
  // is called during the prepare step of glib, and returns a timeout that
  // will be passed to the poll. HandleCheck is called after the poll
  // has completed, and returns whether or not HandleDispatch should be called.
  // HandleDispatch is called if HandleCheck returned true.
  int HandlePrepare();
  bool HandleCheck();
  void HandleDispatch();

  // Adds an Observer, which will start receiving notifications immediately.
  void AddObserver(MessagePumpObserver* observer);

  // Removes an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(MessagePumpObserver* observer);

  // Overridden from MessagePump:
  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) OVERRIDE;

 protected:
  virtual ~MessagePumpGlib();

  // Returns the dispatcher for the current run state (|state_->dispatcher|).
  MessagePumpDispatcher* GetDispatcher();

  ObserverList<MessagePumpObserver>& observers() { return observers_; }

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState;

  RunState* state_;

  // This is a GLib structure that we can add event sources to.  We use the
  // default GLib context, which is the one to which all GTK events are
  // dispatched.
  GMainContext* context_;

  // This is the time when we need to do delayed work.
  TimeTicks delayed_work_time_;

  // The work source.  It is shared by all calls to Run and destroyed when
  // the message pump is destroyed.
  GSource* work_source_;

  // We use a wakeup pipe to make sure we'll get out of the glib polling phase
  // when another thread has scheduled us to do some work.  There is a glib
  // mechanism g_main_context_wakeup, but this won't guarantee that our event's
  // Dispatch() will be called.
  int wakeup_pipe_read_;
  int wakeup_pipe_write_;
  // Use a scoped_ptr to avoid needing the definition of GPollFD in the header.
  scoped_ptr<GPollFD> wakeup_gpollfd_;

  // List of observers.
  ObserverList<MessagePumpObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpGlib);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_H_
