// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GTK_H_
#define BASE_MESSAGE_PUMP_GTK_H_

#include "base/message_pump_glib.h"

typedef union _GdkEvent GdkEvent;
typedef struct _XDisplay Display;

namespace base {

// The documentation for this class is in message_pump_glib.h
class MessagePumpObserver {
 public:
  // This method is called before processing a message.
  virtual void WillProcessEvent(GdkEvent* event) = 0;

  // This method is called after processing a message.
  virtual void DidProcessEvent(GdkEvent* event) = 0;

 protected:
  virtual ~MessagePumpObserver() {}
};

// The documentation for this class is in message_pump_glib.h
//
// The nested loop is exited by either posting a quit, or returning false
// from Dispatch.
class MessagePumpDispatcher {
 public:
  // Dispatches the event. If true is returned processing continues as
  // normal. If false is returned, the nested loop exits immediately.
  virtual bool Dispatch(GdkEvent* event) = 0;

 protected:
  virtual ~MessagePumpDispatcher() {}
};

// This class implements a message-pump for dispatching GTK events.
class BASE_EXPORT MessagePumpGtk : public MessagePumpGlib {
 public:
  MessagePumpGtk();

  // Dispatch an available GdkEvent. Essentially this allows a subclass to do
  // some task before/after calling the default handler (EventDispatcher).
  void DispatchEvents(GdkEvent* event);

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

 protected:
  virtual ~MessagePumpGtk();

 private:
  // Invoked from EventDispatcher. Notifies all observers we're about to
  // process an event.
  void WillProcessEvent(GdkEvent* event);

  // Invoked from EventDispatcher. Notifies all observers we processed an
  // event.
  void DidProcessEvent(GdkEvent* event);

  // Callback prior to gdk dispatching an event.
  static void EventDispatcher(GdkEvent* event, void* data);

  DISALLOW_COPY_AND_ASSIGN(MessagePumpGtk);
};

typedef MessagePumpGtk MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GTK_H_
