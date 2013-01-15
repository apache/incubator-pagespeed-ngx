// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_FOUNDATION_UTIL_H_
#define BASE_MAC_FOUNDATION_UTIL_H_
#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <vector>

#include "base/logging.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else  // __OBJC__
class NSBundle;
class NSString;
#endif  // __OBJC__

class FilePath;

// Adapted from NSPathUtilities.h and NSObjCRuntime.h.
#if __LP64__ || NS_BUILD_32_LIKE_64
typedef unsigned long NSSearchPathDirectory;
typedef unsigned long NSSearchPathDomainMask;
#else
typedef unsigned int NSSearchPathDirectory;
typedef unsigned int NSSearchPathDomainMask;
#endif

namespace base {
namespace mac {

// Returns true if the application is running from a bundle
bool AmIBundled();
void SetOverrideAmIBundled(bool value);

// Returns true if this process is marked as a "Background only process".
bool IsBackgroundOnlyProcess();

// Returns the main bundle or the override, used for code that needs
// to fetch resources from bundles, but work within a unittest where we
// aren't a bundle.
NSBundle* MainAppBundle();
FilePath MainAppBundlePath();

// Returns the path to a resource within the MainAppBundle.
FilePath PathForMainAppBundleResource(CFStringRef resourceName);

// Set the bundle that MainAppBundle will return, overriding the default value
// (Restore the default by calling SetOverrideAppBundle(nil)).
void SetOverrideAppBundle(NSBundle* bundle);
void SetOverrideAppBundlePath(const FilePath& file_path);

// Returns the creator code associated with the CFBundleRef at bundle.
OSType CreatorCodeForCFBundleRef(CFBundleRef bundle);

// Returns the creator code associated with this application, by calling
// CreatorCodeForCFBundleRef for the application's main bundle.  If this
// information cannot be determined, returns kUnknownType ('????').  This
// does not respect the override app bundle because it's based on CFBundle
// instead of NSBundle, and because callers probably don't want the override
// app bundle's creator code anyway.
OSType CreatorCodeForApplication();

// Searches for directories for the given key in only the given |domain_mask|.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                            NSSearchPathDomainMask domain_mask,
                            FilePath* result);

// Searches for directories for the given key in only the local domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetLocalDirectory(NSSearchPathDirectory directory, FilePath* result);

// Searches for directories for the given key in only the user domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result);

// Returns the ~/Library directory.
FilePath GetUserLibraryPath();

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name);

// Utility function to pull out a value from a dictionary, check its type, and
// return it.  Returns NULL if the key is not present or of the wrong type.
CFTypeRef GetValueFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CFTypeID expected_type);

// Retain/release calls for memory management in C++.
void NSObjectRetain(void* obj);
void NSObjectRelease(void* obj);

// CFTypeRefToNSObjectAutorelease transfers ownership of a Core Foundation
// object (one derived from CFTypeRef) to the Foundation memory management
// system.  In a traditional managed-memory environment, cf_object is
// autoreleased and returned as an NSObject.  In a garbage-collected
// environment, cf_object is marked as eligible for garbage collection.
//
// This function should only be used to convert a concrete CFTypeRef type to
// its equivalent "toll-free bridged" NSObject subclass, for example,
// converting a CFStringRef to NSString.
//
// By calling this function, callers relinquish any ownership claim to
// cf_object.  In a managed-memory environment, the object's ownership will be
// managed by the innermost NSAutoreleasePool, so after this function returns,
// callers should not assume that cf_object is valid any longer than the
// returned NSObject.
//
// Returns an id, typed here for C++'s sake as a void*.
void* CFTypeRefToNSObjectAutorelease(CFTypeRef cf_object);

// Returns the base bundle ID, which can be set by SetBaseBundleID but
// defaults to a reasonable string. This never returns NULL. BaseBundleID
// returns a pointer to static storage that must not be freed.
const char* BaseBundleID();

// Sets the base bundle ID to override the default. The implementation will
// make its own copy of new_base_bundle_id.
void SetBaseBundleID(const char* new_base_bundle_id);

}  // namespace mac
}  // namespace base

#if !defined(__OBJC__)
#define OBJC_CPP_CLASS_DECL(x) class x;
#else  // __OBJC__
#define OBJC_CPP_CLASS_DECL(x)
#endif  // __OBJC__

// Convert toll-free bridged CFTypes to NSTypes and vice-versa. This does not
// autorelease |cf_val|. This is useful for the case where there is a CFType in
// a call that expects an NSType and the compiler is complaining about const
// casting problems.
// The calls are used like this:
// NSString *foo = CFToNSCast(CFSTR("Hello"));
// CFStringRef foo2 = NSToCFCast(@"Hello");
// The macro magic below is to enforce safe casting. It could possibly have
// been done using template function specialization, but template function
// specialization doesn't always work intuitively,
// (http://www.gotw.ca/publications/mill17.htm) so the trusty combination
// of macros and function overloading is used instead.

#define CF_TO_NS_CAST_DECL(TypeCF, TypeNS) \
OBJC_CPP_CLASS_DECL(TypeNS) \
\
namespace base { \
namespace mac { \
TypeNS* CFToNSCast(TypeCF##Ref cf_val); \
TypeCF##Ref NSToCFCast(TypeNS* ns_val); \
} \
} \

#define CF_TO_NS_MUTABLE_CAST_DECL(name) \
CF_TO_NS_CAST_DECL(CF##name, NS##name) \
OBJC_CPP_CLASS_DECL(NSMutable##name) \
\
namespace base { \
namespace mac { \
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val); \
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val); \
} \
} \

// List of toll-free bridged types taken from:
// http://www.cocoadev.com/index.pl?TollFreeBridged

CF_TO_NS_MUTABLE_CAST_DECL(Array);
CF_TO_NS_MUTABLE_CAST_DECL(AttributedString);
CF_TO_NS_CAST_DECL(CFCalendar, NSCalendar);
CF_TO_NS_MUTABLE_CAST_DECL(CharacterSet);
CF_TO_NS_MUTABLE_CAST_DECL(Data);
CF_TO_NS_CAST_DECL(CFDate, NSDate);
CF_TO_NS_MUTABLE_CAST_DECL(Dictionary);
CF_TO_NS_CAST_DECL(CFError, NSError);
CF_TO_NS_CAST_DECL(CFLocale, NSLocale);
CF_TO_NS_CAST_DECL(CFNumber, NSNumber);
CF_TO_NS_CAST_DECL(CFRunLoopTimer, NSTimer);
CF_TO_NS_CAST_DECL(CFTimeZone, NSTimeZone);
CF_TO_NS_MUTABLE_CAST_DECL(Set);
CF_TO_NS_CAST_DECL(CFReadStream, NSInputStream);
CF_TO_NS_CAST_DECL(CFWriteStream, NSOutputStream);
CF_TO_NS_MUTABLE_CAST_DECL(String);
CF_TO_NS_CAST_DECL(CFURL, NSURL);

// Stream operations for CFTypes. They can be used with NSTypes as well
// by using the NSToCFCast methods above.
// e.g. LOG(INFO) << base::mac::NSToCFCast(@"foo");
// Operator << can not be overloaded for ObjectiveC types as the compiler
// can not distinguish between overloads for id with overloads for void*.
extern std::ostream& operator<<(std::ostream& o, const CFErrorRef err);
extern std::ostream& operator<<(std::ostream& o, const CFStringRef str);

#endif  // BASE_MAC_FOUNDATION_UTIL_H_
