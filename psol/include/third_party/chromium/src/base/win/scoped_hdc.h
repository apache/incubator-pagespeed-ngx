// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HDC_H_
#define BASE_WIN_SCOPED_HDC_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"

namespace base {
namespace win {

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC.  For an HDC returned by GetDC, use ReleaseDC instead.
class ScopedHDC {
 public:
  ScopedHDC() : hdc_(NULL) { }
  explicit ScopedHDC(HDC h) : hdc_(h) { }

  ~ScopedHDC() {
    Close();
  }

  HDC Get() {
    return hdc_;
  }

  void Set(HDC h) {
    Close();
    hdc_ = h;
  }

  operator HDC() { return hdc_; }

 private:
  void Close() {
#ifdef NOGDI
    assert(false);
#else
    if (hdc_)
      DeleteDC(hdc_);
#endif  // NOGDI
  }

  HDC hdc_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHDC);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_HDC_H_
