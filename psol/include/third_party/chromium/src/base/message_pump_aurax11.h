// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_AURAX11_H
#define BASE_MESSAGE_PUMP_AURAX11_H

#include "base/memory/scoped_ptr.h"
#include "base/message_pump.h"
#include "base/message_pump_glib.h"
#include "base/message_pump_dispatcher.h"
#include "base/message_pump_observer.h"

#include <bitset>
#include <map>
#include <vector>

// It would be nice to include the X11 headers here so that we use Window
// instead of its typedef of unsigned long, but we can't because everything in
// chrome includes us through base/message_loop.h, and X11's crappy #define
// heavy headers muck up half of chrome.

typedef struct _GPollFD GPollFD;
typedef struct _GSource GSource;
typedef struct _XDisplay Display;

namespace base {

// This class implements a message-pump for dispatching X events.
//
// If there's a current dispatcher given through RunWithDispatcher(), that
// dispatcher receives events. Otherwise, we route to messages to dispatchers
// who have subscribed to messages from a specific X11 window.
class BASE_EXPORT MessagePumpAuraX11 : public MessagePumpGlib,
                                       public MessagePumpDispatcher {
 public:
  MessagePumpAuraX11();

  // Returns default X Display.
  static Display* GetDefaultXDisplay();

  // Returns true if the system supports XINPUT2.
  static bool HasXInput2();

  // Returns the UI message pump.
  static MessagePumpAuraX11* Current();

  // Adds/Removes |dispatcher| for the |xid|. This will route all messages from
  // the window |xid| to |dispatcher.
  void AddDispatcherForWindow(MessagePumpDispatcher* dispatcher,
                              unsigned long xid);
  void RemoveDispatcherForWindow(unsigned long xid);

  // Adds/Removes |dispatcher| to receive all events sent to the X root
  // window. A root window can have multiple dispatchers, and events on root
  // windows will be dispatched to all.
  void AddDispatcherForRootWindow(MessagePumpDispatcher* dispatcher);
  void RemoveDispatcherForRootWindow(MessagePumpDispatcher* dispatcher);

  // Internal function. Called by the glib source dispatch function. Processes
  // all available X events.
  bool DispatchXEvents();

  // Blocks on the X11 event queue until we receive notification from the
  // xserver that |w| has been mapped; StructureNotifyMask events on |w| are
  // pulled out from the queue and dispatched out of order.
  //
  // For those that know X11, this is really a wrapper around XWindowEvent
  // which still makes sure the preempted event is dispatched instead of
  // dropped on the floor. This method exists because mapping a window is
  // asynchronous (and we receive an XEvent when mapped), while there are also
  // functions which require a mapped window.
  void BlockUntilWindowMapped(unsigned long xid);

 protected:
  virtual ~MessagePumpAuraX11();

 private:
  typedef std::map<unsigned long, MessagePumpDispatcher*> DispatchersMap;
  typedef std::vector<MessagePumpDispatcher*> Dispatchers;

  // Initializes the glib event source for X.
  void InitXSource();

  // Dispatches the XEvent and returns true if we should exit the current loop
  // of message processing.
  bool ProcessXEvent(MessagePumpDispatcher* dispatcher, XEvent* event);

  // Sends the event to the observers. If an observer returns true, then it does
  // not send the event to any other observers and returns true. Returns false
  // if no observer returns true.
  bool WillProcessXEvent(XEvent* xevent);
  void DidProcessXEvent(XEvent* xevent);

  // Returns the Dispatcher based on the event's target window.
  MessagePumpDispatcher* GetDispatcherForXEvent(
      const base::NativeEvent& xev) const;

  // Overridden from MessagePumpDispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // The event source for X events.
  GSource* x_source_;

  // The poll attached to |x_source_|.
  scoped_ptr<GPollFD> x_poll_;

  DispatchersMap dispatchers_;
  Dispatchers root_window_dispatchers_;

  unsigned long x_root_window_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpAuraX11);
};

typedef MessagePumpAuraX11 MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_AURAX11_H
