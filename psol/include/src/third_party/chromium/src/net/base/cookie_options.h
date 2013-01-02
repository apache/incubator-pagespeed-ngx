// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_BASE_COOKIE_OPTIONS_H_
#define NET_BASE_COOKIE_OPTIONS_H_
#pragma once

namespace net {

class CookieOptions {
 public:
  // Default is to exclude httponly, which means:
  // - reading operations will not return httponly cookies.
  // - writing operations will not write httponly cookies.
  CookieOptions()
      : exclude_httponly_(true),
        force_session_(false) {
  }

  void set_exclude_httponly() { exclude_httponly_ = true; }
  void set_include_httponly() { exclude_httponly_ = false; }
  bool exclude_httponly() const { return exclude_httponly_; }

  // Forces a cookie to be saved as a session cookie. If the expiration time of
  // the cookie is in the past, i.e. the cookie would end up being deleted, this
  // option is ignored. See CookieMonsterTest.ForceSessionOnly.
  void set_force_session() { force_session_ = true; }
  bool force_session() const { return force_session_; }

 private:
  bool exclude_httponly_;
  bool force_session_;
};
}  // namespace net

#endif  // NET_BASE_COOKIE_OPTIONS_H_

