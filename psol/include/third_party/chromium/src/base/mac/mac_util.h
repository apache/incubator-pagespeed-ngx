// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MAC_UTIL_H_
#define BASE_MAC_MAC_UTIL_H_
#pragma once

#include <AvailabilityMacros.h>
#include <Carbon/Carbon.h>
#include <string>

#include "base/logging.h"

// TODO(rohitrao): Clean up sites that include mac_util.h and remove this line.
#include "base/mac/foundation_util.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else  // __OBJC__
class NSImage;
#endif  // __OBJC__

class FilePath;

namespace base {
namespace mac {

// Full screen modes, in increasing order of priority.  More permissive modes
// take predecence.
enum FullScreenMode {
  kFullScreenModeHideAll = 0,
  kFullScreenModeHideDock = 1,
  kFullScreenModeAutoHideAll = 2,
  kNumFullScreenModes = 3,

  // kFullScreenModeNormal is not a valid FullScreenMode, but it is useful to
  // other classes, so we include it here.
  kFullScreenModeNormal = 10,
};

std::string PathFromFSRef(const FSRef& ref);
bool FSRefFromPath(const std::string& path, FSRef* ref);

// Returns an sRGB color space.  The return value is a static value; do not
// release it!
CGColorSpaceRef GetSRGBColorSpace();

// Returns the color space being used by the main display.  The return value
// is a static value; do not release it!
CGColorSpaceRef GetSystemColorSpace();

// Add a full screen request for the given |mode|.  Must be paired with a
// ReleaseFullScreen() call for the same |mode|.  This does not by itself create
// a fullscreen window; rather, it manages per-application state related to
// hiding the dock and menubar.  Must be called on the main thread.
void RequestFullScreen(FullScreenMode mode);

// Release a request for full screen mode.  Must be matched with a
// RequestFullScreen() call for the same |mode|.  As with RequestFullScreen(),
// this does not affect windows directly, but rather manages per-application
// state.  For example, if there are no other outstanding
// |kFullScreenModeAutoHideAll| requests, this will reshow the menu bar.  Must
// be called on main thread.
void ReleaseFullScreen(FullScreenMode mode);

// Convenience method to switch the current fullscreen mode.  This has the same
// net effect as a ReleaseFullScreen(from_mode) call followed immediately by a
// RequestFullScreen(to_mode).  Must be called on the main thread.
void SwitchFullScreenModes(FullScreenMode from_mode, FullScreenMode to_mode);

// Set the visibility of the cursor.
void SetCursorVisibility(bool visible);

// Should windows miniaturize on a double-click (on the title bar)?
bool ShouldWindowsMiniaturizeOnDoubleClick();

// Activates the process with the given PID.
void ActivateProcess(pid_t pid);

// Excludes the file given by |file_path| from being backed up by Time Machine.
bool SetFileBackupExclusion(const FilePath& file_path);

// Sets the process name as displayed in Activity Monitor to process_name.
void SetProcessName(CFStringRef process_name);

// Converts a NSImage to a CGImageRef.  Normally, the system frameworks can do
// this fine, especially on 10.6.  On 10.5, however, CGImage cannot handle
// converting a PDF-backed NSImage into a CGImageRef.  This function will
// rasterize the PDF into a bitmap CGImage.  The caller is responsible for
// releasing the return value.
CGImageRef CopyNSImageToCGImage(NSImage* image);

// Checks if the current application is set as a Login Item, so it will launch
// on Login. If a non-NULL pointer to is_hidden is passed, the Login Item also
// is queried for the 'hide on launch' flag.
bool CheckLoginItemStatus(bool* is_hidden);

// Adds current application to the set of Login Items with specified "hide"
// flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
void AddToLoginItems(bool hide_on_startup);

// Removes the current application from the list Of Login Items.
void RemoveFromLoginItems();

// Returns true if the current process was automatically launched as a
// 'Login Item' with 'hide on startup' flag. Used to suppress opening windows.
bool WasLaunchedAsHiddenLoginItem();

// Run-time OS version checks. Use these instead of
// base::SysInfo::OperatingSystemVersionNumbers. Prefer the "OrEarlier" and
// "OrLater" variants to those that check for a specific version, unless you
// know for sure that you need to check for a specific version.

// Leopard is Mac OS X 10.5, Darwin 9.
bool IsOSLeopard();
bool IsOSLeopardOrEarlier();

// Snow Leopard is Mac OS X 10.6, Darwin 10.
bool IsOSSnowLeopard();
bool IsOSSnowLeopardOrEarlier();
bool IsOSSnowLeopardOrLater();

// Lion is Mac OS X 10.7, Darwin 11.
bool IsOSLion();
bool IsOSLionOrLater();

// This should be infrequently used. It only makes sense to use this to avoid
// codepaths that are very likely to break on future (unreleased, untested,
// unborn) OS releases.
bool IsOSLaterThanLion();

// When the deployment target is set, the code produced cannot run on earlier
// OS releases. That enables some of the IsOS* family to be implemented as
// constant-value inline functions. The MAC_OS_X_VERSION_MIN_REQUIRED macro
// contains the value of the deployment target.

#if defined(MAC_OS_X_VERSION_10_6) && \
    MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6
#define BASE_MAC_MAC_UTIL_H_INLINED_GE_10_6
inline bool IsOSLeopard() { return false; }
inline bool IsOSLeopardOrEarlier() { return false; }
inline bool IsOSSnowLeopardOrLater() { return true; }
#endif

#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
#define BASE_MAC_MAC_UTIL_H_INLINED_GE_10_7
inline bool IsOSSnowLeopard() { return false; }
inline bool IsOSSnowLeopardOrEarlier() { return false; }
inline bool IsOSLionOrLater() { return true; }
#endif

#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_7
#define BASE_MAC_MAC_UTIL_H_INLINED_GT_10_7
inline bool IsOSLion() { return false; }
inline bool IsOSLaterThanLion() { return true; }
#endif

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_MAC_UTIL_H_
