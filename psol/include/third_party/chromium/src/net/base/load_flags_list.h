// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the list of load flags and their values. For the enum values,
// include the file "net/base/load_flags.h".
//
// Here we define the values using a macro LOAD_FLAG, so it can be
// expanded differently in some places (for example, to automatically
// map a load flag value to its symbolic name).

LOAD_FLAG(NORMAL, 0)

// This is "normal reload", meaning an if-none-match/if-modified-since query
LOAD_FLAG(VALIDATE_CACHE, 1 << 0)

// This is "shift-reload", meaning a "pragma: no-cache" end-to-end fetch
LOAD_FLAG(BYPASS_CACHE, 1 << 1)

// This is a back/forward style navigation where the cached content should
// be preferred over any protocol specific cache validation.
LOAD_FLAG(PREFERRING_CACHE, 1 << 2)

// This is a navigation that will fail if it cannot serve the requested
// resource from the cache (or some equivalent local store).
LOAD_FLAG(ONLY_FROM_CACHE, 1 << 3)

// This is a navigation that will not use the cache at all.  It does not
// impact the HTTP request headers.
LOAD_FLAG(DISABLE_CACHE, 1 << 4)

// This is a navigation that will not be intercepted by any registered
// URLRequest::Interceptors.
LOAD_FLAG(DISABLE_INTERCEPT, 1 << 5)

// If present, upload progress messages should be provided to initiator.
LOAD_FLAG(ENABLE_UPLOAD_PROGRESS, 1 << 6)

// If present, collect load timing for the request.
LOAD_FLAG(ENABLE_LOAD_TIMING, 1 << 7)

// If present, ignores certificate mismatches with the domain name.
// (The default behavior is to trigger an OnSSLCertificateError callback.)
LOAD_FLAG(IGNORE_CERT_COMMON_NAME_INVALID, 1 << 8)

// If present, ignores certificate expiration dates
// (The default behavior is to trigger an OnSSLCertificateError callback).
LOAD_FLAG(IGNORE_CERT_DATE_INVALID, 1 << 9)

// If present, trusts all certificate authorities
// (The default behavior is to trigger an OnSSLCertificateError callback).
LOAD_FLAG(IGNORE_CERT_AUTHORITY_INVALID, 1 << 10)

// If present, causes certificate revocation checks to be skipped on secure
// connections.
LOAD_FLAG(DISABLE_CERT_REVOCATION_CHECKING, 1 << 11)

// If present, ignores wrong key usage of the certificate
// (The default behavior is to trigger an OnSSLCertificateError callback).
LOAD_FLAG(IGNORE_CERT_WRONG_USAGE, 1 << 12)

// This load will not make any changes to cookies, including storing new
// cookies or updating existing ones.
LOAD_FLAG(DO_NOT_SAVE_COOKIES, 1 << 13)

// Do not resolve proxies. This override is used when downloading PAC files
// to avoid having a circular dependency.
LOAD_FLAG(BYPASS_PROXY, 1 << 14)

// Indicate this request is for a download, as opposed to viewing.
LOAD_FLAG(IS_DOWNLOAD, 1 << 15)

// Requires EV certificate verification.
LOAD_FLAG(VERIFY_EV_CERT, 1 << 16)

// This load will not send any cookies.
LOAD_FLAG(DO_NOT_SEND_COOKIES, 1 << 17)

// This load will not send authentication data (user name/password)
// to the server (as opposed to the proxy).
LOAD_FLAG(DO_NOT_SEND_AUTH_DATA, 1 << 18)

// This should only be used for testing (set by HttpNetworkTransaction).
LOAD_FLAG(IGNORE_ALL_CERT_ERRORS, 1 << 19)

// Indicate that this is a top level frame, so that we don't assume it is a
// subresource and speculatively pre-connect or pre-resolve when a referring
// page is loaded.
LOAD_FLAG(MAIN_FRAME, 1 << 20)

// Indicate that this is a sub frame, and hence it might have subresources that
// should be speculatively resolved, or even speculatively preconnected.
LOAD_FLAG(SUB_FRAME, 1 << 21)

// If present, intercept actual request/response headers from network stack
// and report them to renderer. This includes cookies, so the flag is only
// respected if renderer has CanReadRawCookies capability in the security
// policy.
LOAD_FLAG(REPORT_RAW_HEADERS, 1 << 22)

// Indicates that this load was motivated by the rel=prefetch feature,
// and is (in theory) not intended for the current frame.
LOAD_FLAG(PREFETCH, 1 << 23)

// Indicates that this is a load that ignores limits and should complete
// immediately.
LOAD_FLAG(IGNORE_LIMITS, 1 << 24)

// Suppress login prompts for this request. Cached credentials or
// default credentials may still be used for authentication.
LOAD_FLAG(DO_NOT_PROMPT_FOR_LOGIN, 1 << 25)

// Indicates that the operation is somewhat likely to be due to an
// explicit user action. This can be used as a hint to treat the
// request with higher priority.
LOAD_FLAG(MAYBE_USER_GESTURE, 1 << 26)
