// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_PROCESS_INFORMATION_H_
#define BASE_WIN_SCOPED_PROCESS_INFORMATION_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/base_export.h"

namespace base {
namespace win {

// Manages the closing of process and thread handles from PROCESS_INFORMATION
// structures. Allows clients to take ownership of either handle independently.
class BASE_EXPORT ScopedProcessInformation {
 public:
  // Creates an instance holding a null PROCESS_INFORMATION.
  ScopedProcessInformation();

  // Closes the held thread and process handles, if any.
  ~ScopedProcessInformation();

  // Returns a pointer that may be passed to API calls such as CreateProcess.
  // DCHECKs that the object is not currently holding any handles.
  // HANDLEs stored in the returned PROCESS_INFORMATION will be owned by this
  // instance.
  PROCESS_INFORMATION* Receive();

  // Returns true iff this instance is holding a thread and/or process handle.
  bool IsValid() const;

  // Closes the held thread and process handles, if any, and resets the held
  // PROCESS_INFORMATION to null.
  void Close();

  // Swaps contents with the other ScopedProcessInformation.
  void Swap(ScopedProcessInformation* other);

  // Populates this instance with duplicate handles and the thread/process IDs
  // from |other|. Returns false in case of failure, in which case this instance
  // will be completely unpopulated.
  bool DuplicateFrom(const ScopedProcessInformation& other);

  // Transfers ownership of the held PROCESS_INFORMATION, if any, away from this
  // instance. Resets the held PROCESS_INFORMATION to null.
  PROCESS_INFORMATION Take();

  // Transfers ownership of the held process handle, if any, away from this
  // instance. The hProcess and dwProcessId members of the held
  // PROCESS_INFORMATION will be reset.
  HANDLE TakeProcessHandle();

  // Transfers ownership of the held thread handle, if any, away from this
  // instance. The hThread and dwThreadId members of the held
  // PROCESS_INFORMATION will be reset.
  HANDLE TakeThreadHandle();

  // Returns the held process handle, if any, while retaining ownership.
  HANDLE process_handle() const {
    return process_information_.hProcess;
  }

  // Returns the held thread handle, if any, while retaining ownership.
  HANDLE thread_handle() const {
    return process_information_.hThread;
  }

  // Returns the held process id, if any.
  DWORD process_id() const {
    return process_information_.dwProcessId;
  }

  // Returns the held thread id, if any.
  DWORD thread_id() const {
    return process_information_.dwThreadId;
  }

 private:
  // Resets the held PROCESS_INFORMATION to null.
  void Reset();

  PROCESS_INFORMATION process_information_;

  DISALLOW_COPY_AND_ASSIGN(ScopedProcessInformation);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_PROCESS_INFORMATION_H_
