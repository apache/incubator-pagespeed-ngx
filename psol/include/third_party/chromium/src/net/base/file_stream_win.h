// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements FileStream for Windows.

#ifndef NET_BASE_FILE_STREAM_WIN_H_
#define NET_BASE_FILE_STREAM_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream_whence.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

class FilePath;

namespace base {
class WaitableEvent;
}

namespace net {

class IOBuffer;

class NET_EXPORT FileStreamWin {
 public:
  explicit FileStreamWin(net::NetLog* net_log);
  FileStreamWin(base::PlatformFile file, int flags, net::NetLog* net_log);
  ~FileStreamWin();

  // FileStream implementations.
  void Close(const CompletionCallback& callback);
  void CloseSync();
  int Open(const FilePath& path, int open_flags,
           const CompletionCallback& callback);
  int OpenSync(const FilePath& path, int open_flags);
  bool IsOpen() const;
  int Seek(Whence whence, int64 offset,
           const Int64CompletionCallback& callback);
  int64 SeekSync(Whence whence, int64 offset);
  int64 Available();
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int ReadSync(char* buf, int buf_len);
  int ReadUntilComplete(char *buf, int buf_len);
  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int WriteSync(const char* buf, int buf_len);
  int64 Truncate(int64 bytes);
  int Flush(const CompletionCallback& callback);
  int FlushSync();
  void EnableErrorStatistics();
  void SetBoundNetLogSource(const net::BoundNetLog& owner_bound_net_log);
  base::PlatformFile GetPlatformFileForTesting();

 private:
  class AsyncContext;

  // A helper method for Seek.
  void SeekFile(Whence whence, int64 offset, int64* result);

  // A helper method for Flush.
  void FlushFile(int* result);

  // Called when the file_ is opened asynchronously. |result| contains the
  // result as a network error code.
  void OnOpened(const CompletionCallback& callback, int* result);

  // Called when the file_ is closed asynchronously.
  void OnClosed(const CompletionCallback& callback);

  // Called when the file_ is seeked asynchronously.
  void OnSeeked(const Int64CompletionCallback& callback, int64* result);

  // Called when the file_ is flushed asynchronously.
  void OnFlushed(const CompletionCallback& callback, int* result);

  // Resets on_io_complete_ and WeakPtr's.
  // Called in OnOpened, OnClosed and OnSeeked.
  void ResetOnIOComplete();

  // Waits until the in-flight async open/close operation is complete.
  void WaitForIOCompletion();

  // This member is used to support asynchronous reads.  It is non-null when
  // the FileStreamWin was opened with PLATFORM_FILE_ASYNC.
  scoped_ptr<AsyncContext> async_context_;

  base::PlatformFile file_;
  int open_flags_;
  bool auto_closed_;
  bool record_uma_;
  net::BoundNetLog bound_net_log_;
  base::WeakPtrFactory<FileStreamWin> weak_ptr_factory_;
  scoped_ptr<base::WaitableEvent> on_io_complete_;

  DISALLOW_COPY_AND_ASSIGN(FileStreamWin);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_WIN_H_
