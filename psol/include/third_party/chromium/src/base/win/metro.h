// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_METRO_H_
#define BASE_WIN_METRO_H_

#include <windows.h>
#include <wpcapi.h>

#include "base/base_export.h"
#include "base/string16.h"

namespace base {
namespace win {

// Identifies the type of the metro launch.
enum MetroLaunchType {
  METRO_LAUNCH,
  METRO_SEARCH,
  METRO_SHARE,
  METRO_FILE,
  METRO_PROTOCOL,
  METRO_LAUNCH_ERROR,
  METRO_LASTLAUNCHTYPE,
};

// In metro mode, this enum identifies the last execution state, i.e. whether
// we crashed, terminated, etc.
enum MetroPreviousExecutionState {
  NOTRUNNING,
  RUNNING,
  SUSPENDED,
  TERMINATED,
  CLOSEDBYUSER,
  LASTEXECUTIONSTATE,
};

// Contains information about the currently displayed tab in metro mode.
struct CurrentTabInfo {
  wchar_t* title;
  wchar_t* url;
};

// Returns the handle to the metro dll loaded in the process. A NULL return
// indicates that the metro dll was not loaded in the process.
BASE_EXPORT HMODULE GetMetroModule();

// Returns true if this process is running as an immersive program
// in Windows Metro mode.
BASE_EXPORT bool IsMetroProcess();

// Returns true if the process identified by the handle passed in is an
// immersive (Metro) process.
BASE_EXPORT bool IsProcessImmersive(HANDLE process);

// Returns true if this process is running under Text Services Framework (TSF)
// and browser must be TSF-aware.
BASE_EXPORT bool IsTsfAwareRequired();

// Sets browser to use Text Service Framework (TSF) regardless of process
// status. On Windows 8, this function also disables CUAS (Cicero Unaware
// Application Support) to emulate Windows Metro mode in terms of IME
// functionality. This should be beneficial in QA process because on can test
// IME functionality in Windows 8 desktop mode.
BASE_EXPORT void SetForceToUseTsf();

// Allocates and returns the destination string via the LocalAlloc API after
// copying the src to it.
BASE_EXPORT wchar_t* LocalAllocAndCopyString(const string16& src);

// Returns true if the screen supports touch.
BASE_EXPORT bool IsTouchEnabled();

// Returns true if Windows Parental control activity logging is enabled. This
// feature is available on Windows Vista and beyond.
// This function should ideally be called on the UI thread.
BASE_EXPORT bool IsParentalControlActivityLoggingOn();

// Returns the type of launch and the activation params. For example if the
// the launch is for METRO_PROTOCOL then the params is a url.
BASE_EXPORT MetroLaunchType GetMetroLaunchParams(string16* params);

// Handler function for the buttons on a metro dialog box
typedef void (*MetroDialogButtonPressedHandler)();

// Handler function invoked when a metro style notification is clicked.
typedef void (*MetroNotificationClickedHandler)(const wchar_t* context);

// Function to display metro style notifications.
typedef void (*MetroNotification)(const char* origin_url,
                                  const char* icon_url,
                                  const wchar_t* title,
                                  const wchar_t* body,
                                  const wchar_t* display_source,
                                  const char* notification_id,
                                  MetroNotificationClickedHandler handler,
                                  const wchar_t* handler_context);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_METRO_H_
