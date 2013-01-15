// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_X_H
#define BASE_MESSAGE_PUMP_GLIB_X_H

#include "base/message_pump.h"
#include "base/message_pump_glib.h"

#include <bitset>

#include <glib.h>
#include <gtk/gtk.h>
#include <X11/X.h>

typedef union _XEvent XEvent;

namespace base {

class MessagePumpGlibX : public MessagePumpForUI {
 public:
  MessagePumpGlibX();
  virtual ~MessagePumpGlibX();

  // Indicates whether a GDK event was injected by chrome (when |true|) or if it
  // was captured and being processed by GDK (when |false|).
  bool IsDispatchingEvent(void) { return dispatching_event_; }

  // Overridden from MessagePumpForUI:
  virtual bool RunOnce(GMainContext* context, bool block);

 private:
  // Some XEvent's can't be directly read from X event queue and will go
  // through GDK's dispatching process and may get discarded. This function
  // sets up a filter to intercept those XEvent's we are interested in
  // and dispatches them so that they won't get lost.
  static GdkFilterReturn GdkEventFilter(GdkXEvent* gxevent,
                                        GdkEvent* gevent,
                                        gpointer data);

  static void EventDispatcherX(GdkEvent* event, gpointer data);

  // Decides whether we are interested in processing this XEvent.
  bool ShouldCaptureXEvent(XEvent* event);

  // Dispatches the XEvent and returns true if we should exit the current loop
  // of message processing.
  bool ProcessXEvent(XEvent* event);

  // Sends the event to the observers. If an observer returns true, then it does
  // not send the event to any other observers and returns true. Returns false
  // if no observer returns true.
  bool WillProcessXEvent(XEvent* xevent);

  // Update the lookup table and flag the events that should be captured and
  // processed so that GDK doesn't get to them.
  void InitializeEventsToCapture(void);

#if defined(HAVE_XINPUT2)
  // Initialize X2 input.
  void InitializeXInput2(void);

  // The opcode used for checking events.
  int xiopcode_;
#endif

  // The event source for GDK events.
  GSource* gdksource_;

  // The default GDK event dispatcher. This is stored so that it can be restored
  // when necessary during nested event dispatching.
  gboolean (*gdkdispatcher_)(GSource*, GSourceFunc, void*);

  // Indicates whether a GDK event was injected by chrome (when |true|) or if it
  // was captured and being processed by GDK (when |false|).
  bool dispatching_event_;

#if ! GTK_CHECK_VERSION(2,18,0)
// GDK_EVENT_LAST was introduced in GTK+ 2.18.0. For earlier versions, we pick a
// large enough value (the value of GDK_EVENT_LAST in 2.18.0) so that it works
// for all versions.
#define GDK_EVENT_LAST 37
#endif

  // We do not want to process all the events ourselves. So we use a lookup
  // table to quickly check if a particular event should be handled by us or if
  // it should be passed on to the default GDK handler.
  std::bitset<LASTEvent> capture_x_events_;
  std::bitset<GDK_EVENT_LAST> capture_gdk_events_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpGlibX);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_X_H
