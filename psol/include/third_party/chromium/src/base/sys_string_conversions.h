// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYS_STRING_CONVERSIONS_H_
#define BASE_SYS_STRING_CONVERSIONS_H_
#pragma once

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/string16.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // OS_MACOSX

namespace base {

class StringPiece;

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
BASE_API std::string SysWideToUTF8(const std::wstring& wide);
BASE_API std::wstring SysUTF8ToWide(const StringPiece& utf8);

// Converts between wide and the system multi-byte representations of a string.
// DANGER: This will lose information and can change (on Windows, this can
// change between reboots).
BASE_API std::string SysWideToNativeMB(const std::wstring& wide);
BASE_API std::wstring SysNativeMBToWide(const StringPiece& native_mb);

// Windows-specific ------------------------------------------------------------

#if defined(OS_WIN)

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
BASE_API std::wstring SysMultiByteToWide(const StringPiece& mb,
                                         uint32 code_page);
BASE_API std::string SysWideToMultiByte(const std::wstring& wide,
                                        uint32 code_page);

#endif  // defined(OS_WIN)

// Mac-specific ----------------------------------------------------------------

#if defined(OS_MACOSX)

// Converts between STL strings and CFStringRefs/NSStrings.

// Creates a string, and returns it with a refcount of 1. You are responsible
// for releasing it. Returns NULL on failure.
CFStringRef SysUTF8ToCFStringRef(const std::string& utf8);
CFStringRef SysUTF16ToCFStringRef(const string16& utf16);
CFStringRef SysWideToCFStringRef(const std::wstring& wide);

// Same, but returns an autoreleased NSString.
NSString* SysUTF8ToNSString(const std::string& utf8);
NSString* SysUTF16ToNSString(const string16& utf16);
NSString* SysWideToNSString(const std::wstring& wide);

// Converts a CFStringRef to an STL string. Returns an empty string on failure.
std::string SysCFStringRefToUTF8(CFStringRef ref);
string16 SysCFStringRefToUTF16(CFStringRef ref);
std::wstring SysCFStringRefToWide(CFStringRef ref);

// Same, but accepts NSString input. Converts nil NSString* to the appropriate
// string type of length 0.
std::string SysNSStringToUTF8(NSString* ref);
string16 SysNSStringToUTF16(NSString* ref);
std::wstring SysNSStringToWide(NSString* ref);

#endif  // defined(OS_MACOSX)

}  // namespace base

#endif  // BASE_SYS_STRING_CONVERSIONS_H_
