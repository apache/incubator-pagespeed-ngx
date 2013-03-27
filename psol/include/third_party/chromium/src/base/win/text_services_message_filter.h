// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_TEXT_SERVICES_MESSAGE_FILTER_H_
#define BASE_WIN_TEXT_SERVICES_MESSAGE_FILTER_H_

#include <msctf.h>
#include <Windows.h>

#include "base/memory/scoped_ptr.h"
#include "base/message_pump_win.h"
#include "base/win/metro.h"
#include "base/win/scoped_comptr.h"

namespace base {
namespace win {

// TextServicesMessageFilter extends MessageFilter with methods that are using
// Text Services Framework COM component.
class BASE_EXPORT TextServicesMessageFilter
    : public base::MessagePumpForUI::MessageFilter {
 public:
  TextServicesMessageFilter();
  virtual ~TextServicesMessageFilter();
  virtual BOOL DoPeekMessage(MSG* msg,
                             HWND window_handle,
                             UINT msg_filter_min,
                             UINT msg_filter_max,
                             UINT remove_msg) OVERRIDE;
  virtual bool ProcessMessage(const MSG& msg) OVERRIDE;

  bool Init();

 private:
  TfClientId client_id_;
  bool is_initialized_;
  base::win::ScopedComPtr<ITfThreadMgr> thread_mgr_;
  base::win::ScopedComPtr<ITfMessagePump> message_pump_;
  base::win::ScopedComPtr<ITfKeystrokeMgr> keystroke_mgr_;

  DISALLOW_COPY_AND_ASSIGN(TextServicesMessageFilter);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_TEXT_SERVICES_MESSAGE_FILTER_H_
