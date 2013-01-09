// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SWITCHES_H_
#define BASE_TEST_TEST_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kLiveOperationTimeout[];
extern const char kTestLargeTimeout[];
extern const char kTestTerminateTimeout[];
extern const char kTestTinyTimeout[];
extern const char kUiTestActionTimeout[];
extern const char kUiTestActionMaxTimeout[];
extern const char kUiTestTerminateTimeout[];
extern const char kUiTestTimeout[];

}  // namespace switches

#endif  // BASE_TEST_TEST_SWITCHES_H_
