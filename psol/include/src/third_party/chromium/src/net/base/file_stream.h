// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream, a basic interface for reading and writing files
// synchronously or asynchronously with support for seeking to an offset.
// Note that even when used asynchronously, only one operation is supported at
// a time.

#ifndef NET_BASE_FILE_STREAM_H_
#define NET_BASE_FILE_STREAM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "net/base/completion_callback.h"
#include "net/base/net_api.h"

class FilePath;

namespace net {

// TODO(darin): Move this to a more generic location.
// This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
enum Whence {
  FROM_BEGIN   = 0,
  FROM_CURRENT = 1,
  FROM_END     = 2
};

class NET_API FileStream {
 public:
  FileStream();

  // Construct a FileStream with an existing file handle and opening flags.
  // |file| is valid file handle.
  // |flags| is a bitfield of base::PlatformFileFlags when the file handle was
  // opened.
  // The already opened file will not be automatically closed when FileStream
  // is destructed.
  FileStream(base::PlatformFile file, int flags);

  ~FileStream();

  // Call this method to close the FileStream.  It is OK to call Close
  // multiple times.  Redundant calls are ignored.
  // Note that if there are any pending async operations, they'll be aborted.
  void Close();

  // Call this method to open the FileStream.  The remaining methods
  // cannot be used unless this method returns OK.  If the file cannot be
  // opened then an error code is returned.
  // open_flags is a bitfield of base::PlatformFileFlags
  int Open(const FilePath& path, int open_flags);

  // Returns true if Open succeeded and Close has not been called.
  bool IsOpen() const;

  // Adjust the position from where data is read.  Upon success, the stream
  // position relative to the start of the file is returned.  Otherwise, an
  // error code is returned.  It is not valid to call Seek while a Read call
  // has a pending completion.
  int64 Seek(Whence whence, int64 offset);

  // Returns the number of bytes available to read from the current stream
  // position until the end of the file.  Otherwise, an error code is returned.
  int64 Available();

  // Call this method to read data from the current stream position.  Up to
  // buf_len bytes will be copied into buf.  (In other words, partial reads are
  // allowed.)  Returns the number of bytes copied, 0 if at end-of-file, or an
  // error code if the operation could not be performed.
  //
  // If opened with PLATFORM_FILE_ASYNC, then a non-null callback
  // must be passed to this method.  In asynchronous mode, if the read could
  // not complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be notified on the current thread (via the MessageLoop) when
  // the read has completed.
  //
  // In the case of an asychronous read, the memory pointed to by |buf| must
  // remain valid until the callback is notified.  However, it is valid to
  // destroy or close the file stream while there is an asynchronous read in
  // progress.  That will cancel the read and allow the buffer to be freed.
  //
  // This method should not be called if the stream was opened WRITE_ONLY.
  //
  // You can pass NULL as the callback for synchronous I/O.
  int Read(char* buf, int buf_len, CompletionCallback* callback);

  // Performs the same as Read, but ensures that exactly buf_len bytes
  // are copied into buf.  A partial read may occur, but only as a result of
  // end-of-file or fatal error.  Returns the number of bytes copied into buf,
  // 0 if at end-of-file and no bytes have been read into buf yet,
  // or an error code if the operation could not be performed.
  int ReadUntilComplete(char *buf, int buf_len);

  // Call this method to write data at the current stream position.  Up to
  // buf_len bytes will be written from buf. (In other words, partial writes are
  // allowed.)  Returns the number of bytes written, or an error code if the
  // operation could not be performed.
  //
  // If opened with PLATFORM_FILE_ASYNC, then a non-null callback
  // must be passed to this method.  In asynchronous mode, if the write could
  // not complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be notified on the current thread (via the MessageLoop) when
  // the write has completed.
  //
  // In the case of an asychronous write, the memory pointed to by |buf| must
  // remain valid until the callback is notified.  However, it is valid to
  // destroy or close the file stream while there is an asynchronous write in
  // progress.  That will cancel the write and allow the buffer to be freed.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  //
  // You can pass NULL as the callback for synchronous I/O.
  int Write(const char* buf, int buf_len, CompletionCallback* callback);

  // Truncates the file to be |bytes| length. This is only valid for writable
  // files. After truncation the file stream is positioned at |bytes|. The new
  // position is retured, or a value < 0 on error.
  // WARNING: one may not truncate a file beyond its current length on any
  //   platform with this call.
  int64 Truncate(int64 bytes);

  // Forces out a filesystem sync on this file to make sure that the file was
  // written out to disk and is not currently sitting in the buffer. This does
  // not have to be called, it just forces one to happen at the time of
  // calling.
  //
  /// Returns an error code if the operation could not be performed.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  int Flush();

 private:
  class AsyncContext;
  friend class AsyncContext;

  // This member is used to support asynchronous reads.  It is non-null when
  // the FileStream was opened with PLATFORM_FILE_ASYNC.
  scoped_ptr<AsyncContext> async_context_;

  base::PlatformFile file_;
  int open_flags_;
  bool auto_closed_;

  DISALLOW_COPY_AND_ASSIGN(FileStream);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_H_
