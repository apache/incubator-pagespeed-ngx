// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
#define BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
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

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif  // OS_MACOSX && !OS_IOS

#if defined(OS_IOS)
#include <objc/runtime.h>
#endif  // OS_IOS

namespace base {

// Class for monitoring various system-related subsystems
// such as power management, network status, etc.
// TODO(mbelshe):  Add support beyond just power management.
class BASE_EXPORT SystemMonitor {
 public:
  // Normalized list of power events.
  enum PowerEvent {
    POWER_STATE_EVENT,  // The Power status of the system has changed.
    SUSPEND_EVENT,      // The system is being suspended.
    RESUME_EVENT        // The system is being resumed.
  };

  // Type of devices whose change need to be monitored, such as add/remove.
  enum DeviceType {
    DEVTYPE_AUDIO_CAPTURE,  // Audio capture device, e.g., microphone.
    DEVTYPE_VIDEO_CAPTURE,  // Video capture device, e.g., webcam.
    DEVTYPE_UNKNOWN,  // Other devices.
  };

  struct BASE_EXPORT RemovableStorageInfo {
    RemovableStorageInfo();
    RemovableStorageInfo(const std::string& id,
                         const string16& device_name,
                         const FilePath::StringType& device_location);

    // Unique device id - persists between device attachments.
    std::string device_id;

    // Human readable removable storage device name.
    string16 name;

    // Current attached removable storage device location.
    FilePath::StringType location;
  };

  // Create SystemMonitor. Only one SystemMonitor instance per application
  // is allowed.
  SystemMonitor();
  ~SystemMonitor();

  // Get the application-wide SystemMonitor (if not present, returns NULL).
  static SystemMonitor* Get();

#if defined(OS_MACOSX)
  // Allocate system resources needed by the SystemMonitor class.
  //
  // This function must be called before instantiating an instance of the class
  // and before the Sandbox is initialized.
#if !defined(OS_IOS)
  static void AllocateSystemIOPorts();
#else
  static void AllocateSystemIOPorts() {}
#endif  // OS_IOS
#endif  // OS_MACOSX

  // Returns information for attached removable storage.
  std::vector<RemovableStorageInfo> GetAttachedRemovableStorage() const;

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
  class BASE_EXPORT PowerObserver {
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

  class BASE_EXPORT DevicesChangedObserver {
   public:
    // Notification that the devices connected to the system have changed.
    // This is only implemented on Windows currently.
    virtual void OnDevicesChanged(DeviceType device_type) {}

    // When a removable storage device is attached or detached, one of these
    // two events is triggered.
    virtual void OnRemovableStorageAttached(
        const std::string& id,
        const string16& name,
        const FilePath::StringType& location) {}
    virtual void OnRemovableStorageDetached(const std::string& id) {}

   protected:
    virtual ~DevicesChangedObserver() {}
  };

  // Add a new observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void AddPowerObserver(PowerObserver* obs);
  void AddDevicesChangedObserver(DevicesChangedObserver* obs);

  // Remove an existing observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void RemovePowerObserver(PowerObserver* obs);
  void RemoveDevicesChangedObserver(DevicesChangedObserver* obs);

  // The ProcessFoo() style methods are a broken pattern and should not
  // be copied. Any significant addition to this class is blocked on
  // refactoring to improve the state of affairs. See http://crbug.com/149059

#if defined(OS_WIN)
  // Windows-specific handling of a WM_POWERBROADCAST message.
  // Embedders of this API should hook their top-level window
  // message loop and forward WM_POWERBROADCAST through this call.
  void ProcessWmPowerBroadcastMessage(int event_id);
#endif

  // Cross-platform handling of a power event.
  void ProcessPowerMessage(PowerEvent event_id);

  // Cross-platform handling of a device change event.
  void ProcessDevicesChanged(DeviceType device_type);
  void ProcessRemovableStorageAttached(const std::string& id,
                                       const string16& name,
                                       const FilePath::StringType& location);
  void ProcessRemovableStorageDetached(const std::string& id);

 private:
  // Mapping of unique device id to device info tuple.
  typedef std::map<std::string, RemovableStorageInfo> RemovableStorageMap;

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
  void NotifyDevicesChanged(DeviceType device_type);
  void NotifyRemovableStorageAttached(const std::string& id,
                                      const string16& name,
                                      const FilePath::StringType& location);
  void NotifyRemovableStorageDetached(const std::string& id);
  void NotifyPowerStateChange();
  void NotifySuspend();
  void NotifyResume();

  scoped_refptr<ObserverListThreadSafe<PowerObserver> > power_observer_list_;
  scoped_refptr<ObserverListThreadSafe<DevicesChangedObserver> >
      devices_changed_observer_list_;
  bool battery_in_use_;
  bool suspended_;

#if defined(ENABLE_BATTERY_MONITORING)
  base::OneShotTimer<SystemMonitor> delayed_battery_check_;
#endif

#if defined(OS_IOS)
  // Holds pointers to system event notification observers.
  std::vector<id> notification_observers_;
#endif

  // For manipulating removable_storage_map_ structure.
  mutable base::Lock removable_storage_lock_;
  // Map of all the attached removable storage devices.
  RemovableStorageMap removable_storage_map_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

}  // namespace base

#endif  // BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_H_
