// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines preprocessor macros for stringizing preprocessor
// symbols (or their output) and manipulating preprocessor symbols
// that define strings.

#ifndef BASE_STRINGIZE_MACROS_H_
#define BASE_STRINGIZE_MACROS_H_
#pragma once

#include "build/build_config.h"

// This is not very useful as it does not expand defined symbols if
// called directly. Use its counterpart without the _NO_EXPANSION
// suffix, below.
#define STRINGIZE_NO_EXPANSION(x) #x

// Use this to quote the provided parameter, first expanding it if it
// is a preprocessor symbol.
//
// For example, if:
//   #define A FOO
//   #define B(x) myobj->FunctionCall(x)
//
// Then:
//   STRINGIZE(A) produces "FOO"
//   STRINGIZE(B(y)) produces "myobj->FunctionCall(y)"
#define STRINGIZE(x) STRINGIZE_NO_EXPANSION(x)

// The following are defined only on Windows (for use when interacting
// with Windows APIs) as wide strings are otherwise deprecated.
#if defined(OS_WIN)

// Second-level utility macros to let us expand symbols.
#define LSTRINGIZE_NO_EXPANSION(x) L ## #x
#define TO_L_STRING_NO_EXPANSION(x) L ## x

// L version of STRINGIZE(). For examples above,
//   LSTRINGIZE(A) produces L"FOO"
//   LSTRINGIZE(B(y)) produces L"myobj->FunctionCall(y)"
#define LSTRINGIZE(x) LSTRINGIZE_NO_EXPANSION(x)

// Adds an L in front of an existing ASCII string constant (after
// expanding symbols). Does not do any quoting.
//
// For example, if:
//   #define C "foo"
//
// Then:
//   TO_L_STRING(C) produces L"foo"
#define TO_L_STRING(x) TO_L_STRING_NO_EXPANSION(x)

#endif  // defined(OS_WIN)

#endif  // BASE_STRINGIZE_MACROS_H_
