// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_DISPATCHER_H
#define BASE_MESSAGE_PUMP_DISPATCHER_H

#include "base/base_export.h"
#include "base/event_types.h"

namespace base {

// Dispatcher is used during a nested invocation of Run to dispatch events when
// |RunLoop(dispatcher).Run()| is used.  If |RunLoop().Run()| is invoked,
// MessageLoop does not dispatch events (or invoke TranslateMessage), rather
// every message is passed to Dispatcher's Dispatch method for dispatch. It is
// up to the Dispatcher whether or not to dispatch the event.
//
// The nested loop is exited by either posting a quit, or returning false
// from Dispatch.
class BASE_EXPORT MessagePumpDispatcher {
 public:
  virtual ~MessagePumpDispatcher() {}

  // Dispatches the event. If true is returned processing continues as
  // normal. If false is returned, the nested loop exits immediately.
  virtual bool Dispatch(const NativeEvent& event) = 0;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_DISPATCHER_H
