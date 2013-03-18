// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream, a basic interface for reading and writing files
// synchronously or asynchronously with support for seeking to an offset.
// Note that even when used asynchronously, only one operation is supported at
// a time.

#ifndef NET_BASE_FILE_STREAM_H_
#define NET_BASE_FILE_STREAM_H_

#include "base/platform_file.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream_whence.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#if defined(OS_WIN)
#include "net/base/file_stream_win.h"
#elif defined(OS_POSIX)
#include "net/base/file_stream_posix.h"
#endif

class FilePath;

namespace net {

class IOBuffer;

class NET_EXPORT FileStream {
 public:
  // Creates a |FileStream| with a new |BoundNetLog| (based on |net_log|)
  // attached.  |net_log| may be NULL if no logging is needed.
  explicit FileStream(net::NetLog* net_log);

  // Construct a FileStream with an existing file handle and opening flags.
  // |file| is valid file handle.
  // |flags| is a bitfield of base::PlatformFileFlags when the file handle was
  // opened.
  // |net_log| is the net log pointer to use to create a |BoundNetLog|.  May be
  // NULL if logging is not needed.
  // The already opened file will not be automatically closed when FileStream
  // is destructed.
  FileStream(base::PlatformFile file, int flags, net::NetLog* net_log);

  // If the file stream was opened with Open() or OpenSync(), the underlying
  // file will be closed automatically by the destructor, if not closed
  // manually.
  virtual ~FileStream();

  // Call this method to close the FileStream, which was previously opened in
  // the async mode (PLATFORM_FILE_ASYNC) asynchronously.
  //
  // Once the operation is done, |callback| will be run on the thread where
  // Close() was called, with OK (i.e. an error is not propagated just like
  // CloseSync() does not).
  //
  // It is not OK to call Close() multiple times. The behavior is not defined.
  // Note that there must never be any pending async operations.
  virtual void Close(const CompletionCallback& callback);

  // Call this method to close the FileStream synchronously.
  // It is OK to call CloseSync() multiple times.  Redundant calls are
  // ignored. Note that if there are any pending async operations, they'll
  // be aborted.
  virtual void CloseSync();

  // Call this method to open the FileStream asynchronously.  The remaining
  // methods cannot be used unless the file is opened successfully. Returns
  // ERR_IO_PENDING if the operation is started. If the operation cannot be
  // started then an error code is returned.
  //
  // Once the operation is done, |callback| will be run on the thread where
  // Open() was called, with the result code. open_flags is a bitfield of
  // base::PlatformFileFlags.
  //
  // If the file stream is not closed manually, the underlying file will be
  // automatically closed when FileStream is destructed in an asynchronous
  // manner (i.e. the file stream is closed in the background but you don't
  // know when).
  virtual int Open(const FilePath& path, int open_flags,
                   const CompletionCallback& callback);

  // Call this method to open the FileStream synchronously.
  // The remaining methods cannot be used unless this method returns OK.  If
  // the file cannot be opened then an error code is returned.  open_flags is
  // a bitfield of base::PlatformFileFlags
  //
  // If the file stream is not closed manually, the underlying file will be
  // automatically closed when FileStream is destructed.
  virtual int OpenSync(const FilePath& path, int open_flags);

  // Returns true if Open succeeded and Close has not been called.
  virtual bool IsOpen() const;

  // Adjust the position from where data is read asynchronously.
  // Upon success, ERR_IO_PENDING is returned and |callback| will be run
  // on the thread where Seek() was called with the the stream position
  // relative to the start of the file.  Otherwise, an error code is returned.
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  virtual int Seek(Whence whence, int64 offset,
                   const Int64CompletionCallback& callback);

  // Adjust the position from where data is read synchronously.
  // Upon success, the stream position relative to the start of the file is
  // returned.  Otherwise, an error code is returned.  It is not valid to
  // call SeekSync while a Read call has a pending completion.
  virtual int64 SeekSync(Whence whence, int64 offset);

  // Returns the number of bytes available to read from the current stream
  // position until the end of the file.  Otherwise, an error code is returned.
  virtual int64 Available();

  // Call this method to read data from the current stream position
  // asynchronously. Up to buf_len bytes will be copied into buf.  (In
  // other words, partial reads are allowed.)  Returns the number of bytes
  // copied, 0 if at end-of-file, or an error code if the operation could
  // not be performed.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the read could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Read() was called, when the
  // read has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous read in progress.  That will cancel the read and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened WRITE_ONLY.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback);

  // Call this method to read data from the current stream position
  // synchronously. Up to buf_len bytes will be copied into buf.  (In
  // other words, partial reads are allowed.)  Returns the number of bytes
  // copied, 0 if at end-of-file, or an error code if the operation could
  // not be performed.
  //
  // The file must not be opened with PLATFORM_FILE_ASYNC.
  // This method must not be called if the stream was opened WRITE_ONLY.
  virtual int ReadSync(char* buf, int buf_len);

  // Performs the same as ReadSync, but ensures that exactly buf_len bytes
  // are copied into buf.  A partial read may occur, but only as a result of
  // end-of-file or fatal error.  Returns the number of bytes copied into buf,
  // 0 if at end-of-file and no bytes have been read into buf yet,
  // or an error code if the operation could not be performed.
  virtual int ReadUntilComplete(char *buf, int buf_len);

  // Call this method to write data at the current stream position
  // asynchronously.  Up to buf_len bytes will be written from buf. (In
  // other words, partial writes are allowed.)  Returns the number of
  // bytes written, or an error code if the operation could not be
  // performed.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the write could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Write() was called when
  // the write has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous write in progress.  That will cancel the write and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened READ_ONLY.
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback);

  // Call this method to write data at the current stream position
  // synchronously.  Up to buf_len bytes will be written from buf. (In
  // other words, partial writes are allowed.)  Returns the number of
  // bytes written, or an error code if the operation could not be
  // performed.
  //
  // The file must not be opened with PLATFORM_FILE_ASYNC.
  // This method must not be called if the stream was opened READ_ONLY.
  virtual int WriteSync(const char* buf, int buf_len);

  // Truncates the file to be |bytes| length. This is only valid for writable
  // files. After truncation the file stream is positioned at |bytes|. The new
  // position is returned, or a value < 0 on error.
  // WARNING: one may not truncate a file beyond its current length on any
  //   platform with this call.
  virtual int64 Truncate(int64 bytes);

  // Forces out a filesystem sync on this file to make sure that the file was
  // written out to disk and is not currently sitting in the buffer. This does
  // not have to be called, it just forces one to happen at the time of
  // calling.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the write could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Flush() was called when
  // the write has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous flush in progress.  That will cancel the flush and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  virtual int Flush(const CompletionCallback& callback);

  // Forces out a filesystem sync on this file to make sure that the file was
  // written out to disk and is not currently sitting in the buffer. This does
  // not have to be called, it just forces one to happen at the time of
  // calling.
  //
  // Returns an error code if the operation could not be performed.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  virtual int FlushSync();

  // Turns on UMA error statistics gathering.
  void EnableErrorStatistics();

  // Sets the source reference for net-internals logging.
  // Creates source dependency events between |owner_bound_net_log| and
  // |bound_net_log_|.  Each gets an event showing the dependency on the other.
  // If only one of those is valid, it gets an event showing that a change
  // of ownership happened, but without details.
  void SetBoundNetLogSource(const net::BoundNetLog& owner_bound_net_log);

  // Returns the underlying platform file for testing.
  base::PlatformFile GetPlatformFileForTesting();

 private:
#if defined(OS_WIN)
  FileStreamWin impl_;
#elif defined(OS_POSIX)
  FileStreamPosix impl_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FileStream);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_H_
