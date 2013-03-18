// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SHORTCUT_H_
#define BASE_WIN_SHORTCUT_H_

#include <windows.h>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/string16.h"

namespace base {
namespace win {

enum ShortcutOperation {
  // Create a new shortcut (overwriting if necessary).
  SHORTCUT_CREATE_ALWAYS = 0,
  // Overwrite an existing shortcut (fails if the shortcut doesn't exist).
  SHORTCUT_REPLACE_EXISTING,
  // Update specified properties only on an existing shortcut.
  SHORTCUT_UPDATE_EXISTING,
};

// Properties for shortcuts. Properties set will be applied to the shortcut on
// creation/update, others will be ignored.
// Callers are encouraged to use the setters provided which take care of
// setting |options| as desired.
struct ShortcutProperties {
  enum IndividualProperties {
    PROPERTIES_TARGET = 1 << 0,
    PROPERTIES_WORKING_DIR = 1 << 1,
    PROPERTIES_ARGUMENTS = 1 << 2,
    PROPERTIES_DESCRIPTION = 1 << 3,
    PROPERTIES_ICON = 1 << 4,
    PROPERTIES_APP_ID = 1 << 5,
    PROPERTIES_DUAL_MODE = 1 << 6,
  };

  ShortcutProperties() : icon_index(-1), dual_mode(false), options(0U) {}

  void set_target(const FilePath& target_in) {
    target = target_in;
    options |= PROPERTIES_TARGET;
  }

  void set_working_dir(const FilePath& working_dir_in) {
    working_dir = working_dir_in;
    options |= PROPERTIES_WORKING_DIR;
  }

  void set_arguments(const string16& arguments_in) {
    // Size restriction as per MSDN at http://goo.gl/TJ7q5.
    DCHECK(arguments_in.size() < MAX_PATH);
    arguments = arguments_in;
    options |= PROPERTIES_ARGUMENTS;
  }

  void set_description(const string16& description_in) {
    // Size restriction as per MSDN at http://goo.gl/OdNQq.
    DCHECK(description_in.size() < MAX_PATH);
    description = description_in;
    options |= PROPERTIES_DESCRIPTION;
  }

  void set_icon(const FilePath& icon_in, int icon_index_in) {
    icon = icon_in;
    icon_index = icon_index_in;
    options |= PROPERTIES_ICON;
  }

  void set_app_id(const string16& app_id_in) {
    app_id = app_id_in;
    options |= PROPERTIES_APP_ID;
  }

  void set_dual_mode(bool dual_mode_in) {
    dual_mode = dual_mode_in;
    options |= PROPERTIES_DUAL_MODE;
  }

  // The target to launch from this shortcut. This is mandatory when creating
  // a shortcut.
  FilePath target;
  // The name of the working directory when launching the shortcut.
  FilePath working_dir;
  // The arguments to be applied to |target| when launching from this shortcut.
  // The length of this string must be less than MAX_PATH.
  string16 arguments;
  // The localized description of the shortcut.
  // The length of this string must be less than MAX_PATH.
  string16 description;
  // The path to the icon (can be a dll or exe, in which case |icon_index| is
  // the resource id).
  FilePath icon;
  int icon_index;
  // The app model id for the shortcut (Win7+).
  string16 app_id;
  // Whether this is a dual mode shortcut (Win8+).
  bool dual_mode;
  // Bitfield made of IndividualProperties. Properties set in |options| will be
  // set on the shortcut, others will be ignored.
  uint32 options;
};

// This method creates (or updates) a shortcut link at |shortcut_path| using the
// information given through |properties|.
// Ensure you have initialized COM before calling into this function.
// |operation|: a choice from the ShortcutOperation enum.
// If |operation| is SHORTCUT_REPLACE_EXISTING or SHORTCUT_UPDATE_EXISTING and
// |shortcut_path| does not exist, this method is a no-op and returns false.
BASE_EXPORT bool CreateOrUpdateShortcutLink(
    const FilePath& shortcut_path,
    const ShortcutProperties& properties,
    ShortcutOperation operation);

// Resolve Windows shortcut (.LNK file)
// This methods tries to resolve a shortcut .LNK file. The path of the shortcut
// to resolve is in |shortcut_path|. If |target_path| is not NULL, the target
// will be resolved and placed in |target_path|. If |args| is not NULL, the
// arguments will be retrieved and placed in |args|. The function returns true
// if all requested fields are found successfully.
// Callers can safely use the same variable for both |shortcut_path| and
// |target_path|.
BASE_EXPORT bool ResolveShortcut(const FilePath& shortcut_path,
                                 FilePath* target_path,
                                 string16* args);

// Pins a shortcut to the Windows 7 taskbar. The shortcut file must already
// exist and be a shortcut that points to an executable.
BASE_EXPORT bool TaskbarPinShortcutLink(const wchar_t* shortcut);

// Unpins a shortcut from the Windows 7 taskbar. The shortcut must exist and
// already be pinned to the taskbar.
BASE_EXPORT bool TaskbarUnpinShortcutLink(const wchar_t* shortcut);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SHORTCUT_H_
