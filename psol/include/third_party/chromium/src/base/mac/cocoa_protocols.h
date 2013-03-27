// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_COCOA_PROTOCOLS_MAC_H_
#define BASE_COCOA_PROTOCOLS_MAC_H_

#import <Cocoa/Cocoa.h>

// GTM also maintinas a list of empty protocols, but only the ones the library
// requires. Augment that below.
#import "third_party/GTM/GTMDefines.h"

// New Mac OS X SDKs introduce new protocols used for delegates.  These
// protocol defintions aren't not present in earlier releases of the Mac OS X
// SDK.  In order to support building against the new SDK, which requires
// delegates to conform to these protocols, and earlier SDKs, which do not
// define these protocols at all, this file will provide empty protocol
// definitions when used with earlier SDK versions.

#define DEFINE_EMPTY_PROTOCOL(p) \
@protocol p \
@end

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

DEFINE_EMPTY_PROTOCOL(NSDraggingDestination)

#endif  // MAC_OS_X_VERSION_10_7

#if !defined(MAC_OS_X_VERSION_10_8) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8

DEFINE_EMPTY_PROTOCOL(NSUserNotificationCenterDelegate)

#endif  // MAC_OS_X_VERSION_10_8

#undef DEFINE_EMPTY_PROTOCOL

#endif  // BASE_COCOA_PROTOCOLS_MAC_H_
