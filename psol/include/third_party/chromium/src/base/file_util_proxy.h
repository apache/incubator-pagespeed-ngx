// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_UTIL_PROXY_H_
#define BASE_FILE_UTIL_PROXY_H_

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"

namespace tracked_objects {
class Location;
};

namespace base {

class TaskRunner;
class Time;

// This class provides asynchronous access to common file routines.
class BASE_EXPORT FileUtilProxy {
 public:
  // Holds metadata for file or directory entry.
  struct Entry {
    FilePath::StringType name;
    bool is_directory;
    int64 size;
    base::Time last_modified_time;
  };

  // This callback is used by methods that report only an error code.  It is
  // valid to pass a null callback to any function that takes a StatusCallback,
  // in which case the operation will complete silently.
  typedef Callback<void(PlatformFileError)> StatusCallback;

  typedef Callback<void(PlatformFileError,
                        PassPlatformFile,
                        bool /* created */)> CreateOrOpenCallback;
  typedef Callback<void(PlatformFileError,
                        PassPlatformFile,
                        const FilePath&)> CreateTemporaryCallback;
  typedef Callback<void(PlatformFileError,
                        const PlatformFileInfo&)> GetFileInfoCallback;
  typedef Callback<void(PlatformFileError,
                        const char* /* data */,
                        int /* bytes read */)> ReadCallback;
  typedef Callback<void(PlatformFileError,
                        int /* bytes written */)> WriteCallback;

  typedef Callback<PlatformFileError(PlatformFile*, bool*)> CreateOrOpenTask;
  typedef Callback<PlatformFileError(PlatformFile)> CloseTask;
  typedef Callback<PlatformFileError(void)> FileTask;

  // Creates or opens a file with the given flags. It is invalid to pass a null
  // callback. If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to
  // create a new file at the given |file_path| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |file_path| already exists.
  static bool CreateOrOpen(TaskRunner* task_runner,
                           const FilePath& file_path,
                           int file_flags,
                           const CreateOrOpenCallback& callback);

  // Creates a temporary file for writing. The path and an open file handle are
  // returned. It is invalid to pass a null callback. The additional file flags
  // will be added on top of the default file flags which are:
  //   base::PLATFORM_FILE_CREATE_ALWAYS
  //   base::PLATFORM_FILE_WRITE
  //   base::PLATFORM_FILE_TEMPORARY.
  // Set |additional_file_flags| to 0 for synchronous writes and set to
  // base::PLATFORM_FILE_ASYNC to support asynchronous file operations.
  static bool CreateTemporary(
      TaskRunner* task_runner,
      int additional_file_flags,
      const CreateTemporaryCallback& callback);

  // Close the given file handle.
  static bool Close(TaskRunner* task_runner,
                    PlatformFile,
                    const StatusCallback& callback);

  // Retrieves the information about a file. It is invalid to pass a null
  // callback.
  static bool GetFileInfo(
      TaskRunner* task_runner,
      const FilePath& file_path,
      const GetFileInfoCallback& callback);

  static bool GetFileInfoFromPlatformFile(
      TaskRunner* task_runner,
      PlatformFile file,
      const GetFileInfoCallback& callback);

  // Deletes a file or a directory.
  // It is an error to delete a non-empty directory with recursive=false.
  static bool Delete(TaskRunner* task_runner,
                     const FilePath& file_path,
                     bool recursive,
                     const StatusCallback& callback);

  // Deletes a directory and all of its contents.
  static bool RecursiveDelete(
      TaskRunner* task_runner,
      const FilePath& file_path,
      const StatusCallback& callback);

  // Reads from a file. On success, the file pointer is moved to position
  // |offset + bytes_to_read| in the file. The callback can be null.
  static bool Read(
      TaskRunner* task_runner,
      PlatformFile file,
      int64 offset,
      int bytes_to_read,
      const ReadCallback& callback);

  // Writes to a file. If |offset| is greater than the length of the file,
  // |false| is returned. On success, the file pointer is moved to position
  // |offset + bytes_to_write| in the file. The callback can be null.
  // |bytes_to_write| must be greater than zero.
  static bool Write(
      TaskRunner* task_runner,
      PlatformFile file,
      int64 offset,
      const char* buffer,
      int bytes_to_write,
      const WriteCallback& callback);

  // Touches a file. The callback can be null.
  static bool Touch(
      TaskRunner* task_runner,
      PlatformFile file,
      const Time& last_access_time,
      const Time& last_modified_time,
      const StatusCallback& callback);

  // Touches a file. The callback can be null.
  static bool Touch(
      TaskRunner* task_runner,
      const FilePath& file_path,
      const Time& last_access_time,
      const Time& last_modified_time,
      const StatusCallback& callback);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be null.
  static bool Truncate(
      TaskRunner* task_runner,
      PlatformFile file,
      int64 length,
      const StatusCallback& callback);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be null.
  static bool Truncate(
      TaskRunner* task_runner,
      const FilePath& path,
      int64 length,
      const StatusCallback& callback);

  // Flushes a file. The callback can be null.
  static bool Flush(
      TaskRunner* task_runner,
      PlatformFile file,
      const StatusCallback& callback);

  // Relay helpers.
  static bool RelayFileTask(
      TaskRunner* task_runner,
      const tracked_objects::Location& from_here,
      const FileTask& task,
      const StatusCallback& callback);

  static bool RelayCreateOrOpen(
      TaskRunner* task_runner,
      const CreateOrOpenTask& open_task,
      const CloseTask& close_task,
      const CreateOrOpenCallback& callback);

  static bool RelayClose(
      TaskRunner* task_runner,
      const CloseTask& close_task,
      PlatformFile,
      const StatusCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilProxy);
};

}  // namespace base

#endif  // BASE_FILE_UTIL_PROXY_H_
