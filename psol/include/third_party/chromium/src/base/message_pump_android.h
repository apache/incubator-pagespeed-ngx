// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_ANDROID_H_
#define BASE_MESSAGE_PUMP_ANDROID_H_

#include <jni.h>

#include "base/compiler_specific.h"
#include "base/message_pump.h"

namespace base {

class RunLoop;
class TimeTicks;

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_ANDROID platform.
class MessagePumpForUI : public MessagePump {
 public:
  MessagePumpForUI();

  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) OVERRIDE;

  virtual void Start(Delegate* delegate);

  static bool RegisterBindings(JNIEnv* env);

 protected:
  virtual ~MessagePumpForUI();

 private:
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_ANDROID_H_
