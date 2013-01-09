// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
#define BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
#pragma once

#include "base/base_api.h"
#include "base/basictypes.h"
#include "build/build_config.h"

// Windows HiRes timers drain the battery faster so we need to know the battery
// status.  This isn't true for other platforms.
#if defined(OS_WIN)
#define ENABLE_BATTERY_MONITORING 1
#else
#undef ENABLE_BATTERY_MONITORING
#endif  // !OS_WIN

#include "base/observer_list_threadsafe.h"
#if defined(ENABLE_BATTERY_MONITORING)
#include "base/timer.h"
#endif  // defined(ENABLE_BATTERY_MONITORING)

#if defined(OS_MACOSX)
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif  // OS_MACOSX

namespace base {

// Class for monitoring various system-related subsystems
// such as power management, network status, etc.
// TODO(mbelshe):  Add support beyond just power management.
class BASE_API SystemMonitor {
 public:
  // Normalized list of power events.
  enum PowerEvent {
    POWER_STATE_EVENT,  // The Power status of the system has changed.
    SUSPEND_EVENT,      // The system is being suspended.
    RESUME_EVENT        // The system is being resumed.
  };

  // Create SystemMonitor. Only one SystemMonitor instance per application
  // is allowed.
  SystemMonitor();
  ~SystemMonitor();

  // Get the application-wide SystemMonitor (if not present, returns NULL).
  static SystemMonitor* Get();

  //
  // Power-related APIs
  //

  // Is the computer currently on battery power.
  // Can be called on any thread.
  bool BatteryPower() const {
    // Using a lock here is not necessary for just a bool.
    return battery_in_use_;
  }

  // Callbacks will be called on the thread which creates the SystemMonitor.
  // During the callback, Add/RemoveObserver will block until the callbacks
  // are finished. Observers should implement quick callback functions; if
  // lengthy operations are needed, the observer should take care to invoke
  // the operation on an appropriate thread.
  class BASE_API PowerObserver {
   public:
    // Notification of a change in power status of the computer, such
    // as from switching between battery and A/C power.
    virtual void OnPowerStateChange(bool on_battery_power) {}

    // Notification that the system is suspending.
    virtual void OnSuspend() {}

    // Notification that the system is resuming.
    virtual void OnResume() {}

   protected:
    virtual ~PowerObserver() {}
  };

  // Add a new observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void AddObserver(PowerObserver* obs);

  // Remove an existing observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void RemoveObserver(PowerObserver* obs);

#if defined(OS_WIN)
  // Windows-specific handling of a WM_POWERBROADCAST message.
  // Embedders of this API should hook their top-level window
  // message loop and forward WM_POWERBROADCAST through this call.
  void ProcessWmPowerBroadcastMessage(int event_id);
#endif

  // Cross-platform handling of a power event.
  void ProcessPowerMessage(PowerEvent event_id);

 private:
#if defined(OS_MACOSX)
  void PlatformInit();
  void PlatformDestroy();
#endif

  // Platform-specific method to check whether the system is currently
  // running on battery power.  Returns true if running on batteries,
  // false otherwise.
  bool IsBatteryPower();

  // Checks the battery status and notifies observers if the battery
  // status has changed.
  void BatteryCheck();

  // Functions to trigger notifications.
  void NotifyPowerStateChange();
  void NotifySuspend();
  void NotifyResume();

  scoped_refptr<ObserverListThreadSafe<PowerObserver> > observer_list_;
  bool battery_in_use_;
  bool suspended_;

#if defined(ENABLE_BATTERY_MONITORING)
  base::OneShotTimer<SystemMonitor> delayed_battery_check_;
#endif

#if defined(OS_MACOSX)
  IONotificationPortRef notification_port_ref_;
  io_object_t notifier_object_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

}  // namespace base

#endif  // BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
