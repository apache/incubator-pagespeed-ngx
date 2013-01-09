// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_FILE_UTIL_ICU_H_
#define BASE_I18N_FILE_UTIL_ICU_H_
#pragma once

// File utilities that use the ICU library go in this file.

#include "base/file_path.h"
#include "base/string16.h"

namespace file_util {

// Returns true if file_name does not have any illegal character. The input
// param has the same restriction as that for ReplaceIllegalCharacters.
bool IsFilenameLegal(const string16& file_name);

// Replaces characters in 'file_name' that are illegal for file names with
// 'replace_char'. 'file_name' must not be a full or relative path, but just the
// file name component (since slashes are considered illegal). Any leading or
// trailing whitespace in 'file_name' is removed.
// Example:
//   file_name == "bad:file*name?.txt", changed to: "bad-file-name-.txt" when
//   'replace_char' is '-'.
void ReplaceIllegalCharactersInPath(FilePath::StringType* file_name,
                                    char replace_char);

// Compares two filenames using the current locale information. This can be
// used to sort directory listings. It behaves like "operator<" for use in
// std::sort.
bool LocaleAwareCompareFilenames(const FilePath& a, const FilePath& b);

}  // namespace file_util

#endif  // BASE_I18N_FILE_UTIL_ICU_H_
