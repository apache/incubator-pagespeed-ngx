// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SHORTCUT_WIN_H_
#define BASE_TEST_TEST_SHORTCUT_WIN_H_

#include "base/file_path.h"
#include "base/win/shortcut.h"

// Windows shortcut functions used only by tests.

namespace base {
namespace win {

// Validates that a shortcut exists at |shortcut_path| with the expected
// |properties|.
// Logs gtest failures on failed verifications.
void ValidateShortcut(const FilePath& shortcut_path,
                      const ShortcutProperties& properties);

}  // namespace win
}  // namespace base

#endif  // BASE_TEST_TEST_SHORTCUT_WIN_H_
