// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHARED_MEMORY_H_
#define BASE_SHARED_MEMORY_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <semaphore.h>
#include "base/file_descriptor_posix.h"
#endif
#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/process.h"

class FilePath;

namespace base {

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
#if defined(OS_WIN)
typedef HANDLE SharedMemoryHandle;
typedef HANDLE SharedMemoryLock;
#elif defined(OS_POSIX)
// A SharedMemoryId is sufficient to identify a given shared memory segment on a
// system, but insufficient to map it.
typedef FileDescriptor SharedMemoryHandle;
typedef ino_t SharedMemoryId;
// On POSIX, the lock is implemented as a lockf() on the mapped file,
// so no additional member (or definition of SharedMemoryLock) is
// needed.
#endif

// Platform abstraction for shared memory.  Provides a C++ wrapper
// around the OS primitive for a memory mapped file.
class BASE_API SharedMemory {
 public:
  SharedMemory();

#if defined(OS_WIN)
  // Similar to the default constructor, except that this allows for
  // calling Lock() to acquire the named mutex before either Create or Open
  // are called on Windows.
  explicit SharedMemory(const std::wstring& name);
#endif

  // Create a new SharedMemory object from an existing, open
  // shared memory file.
  SharedMemory(SharedMemoryHandle handle, bool read_only);

  // Create a new SharedMemory object from an existing, open
  // shared memory file that was created by a remote process and not shared
  // to the current process.
  SharedMemory(SharedMemoryHandle handle, bool read_only,
               ProcessHandle process);

  // Closes any open files.
  ~SharedMemory();

  // Return true iff the given handle is valid (i.e. not the distingished
  // invalid value; NULL for a HANDLE and -1 for a file descriptor)
  static bool IsHandleValid(const SharedMemoryHandle& handle);

  // Returns invalid handle (see comment above for exact definition).
  static SharedMemoryHandle NULLHandle();

  // Closes a shared memory handle.
  static void CloseHandle(const SharedMemoryHandle& handle);

  // Creates and maps an anonymous shared memory segment of size size.
  // Returns true on success and false on failure.
  bool CreateAndMapAnonymous(uint32 size);

  // Creates an anonymous shared memory segment of size size.
  // Returns true on success and false on failure.
  bool CreateAnonymous(uint32 size);

  // Creates or opens a shared memory segment based on a name.
  // If open_existing is true, and the shared memory already exists,
  // opens the existing shared memory and ignores the size parameter.
  // If open_existing is false, shared memory must not exist.
  // size is the size of the block to be created.
  // Returns true on success, false on failure.
  bool CreateNamed(const std::string& name, bool open_existing, uint32 size);

  // Deletes resources associated with a shared memory segment based on name.
  // Not all platforms require this call.
  bool Delete(const std::string& name);

  // Opens a shared memory segment based on a name.
  // If read_only is true, opens for read-only access.
  // Returns true on success, false on failure.
  bool Open(const std::string& name, bool read_only);

  // Maps the shared memory into the caller's address space.
  // Returns true on success, false otherwise.  The memory address
  // is accessed via the memory() accessor.
  bool Map(uint32 bytes);

  // Unmaps the shared memory from the caller's address space.
  // Returns true if successful; returns false on error or if the
  // memory is not mapped.
  bool Unmap();

  // Get the size of the shared memory backing file.
  // Note:  This size is only available to the creator of the
  // shared memory, and not to those that opened shared memory
  // created externally.
  // Returns 0 if not created or unknown.
  // Deprecated method, please keep track of the size yourself if you created
  // it.
  // http://crbug.com/60821
  uint32 created_size() const { return created_size_; }

  // Gets a pointer to the opened memory space if it has been
  // Mapped via Map().  Returns NULL if it is not mapped.
  void *memory() const { return memory_; }

  // Returns the underlying OS handle for this segment.
  // Use of this handle for anything other than an opaque
  // identifier is not portable.
  SharedMemoryHandle handle() const;

#if defined(OS_POSIX)
  // Returns a unique identifier for this shared memory segment. Inode numbers
  // are technically only unique to a single filesystem. However, we always
  // allocate shared memory backing files from the same directory, so will end
  // up on the same filesystem.
  SharedMemoryId id() const { return inode_; }
#endif

  // Closes the open shared memory segment.
  // It is safe to call Close repeatedly.
  void Close();

  // Shares the shared memory to another process.  Attempts
  // to create a platform-specific new_handle which can be
  // used in a remote process to access the shared memory
  // file.  new_handle is an ouput parameter to receive
  // the handle for use in the remote process.
  // Returns true on success, false otherwise.
  bool ShareToProcess(ProcessHandle process,
                      SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(process, new_handle, false);
  }

  // Logically equivalent to:
  //   bool ok = ShareToProcess(process, new_handle);
  //   Close();
  //   return ok;
  // Note that the memory is unmapped by calling this method, regardless of the
  // return value.
  bool GiveToProcess(ProcessHandle process,
                     SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(process, new_handle, true);
  }

  // Locks the shared memory.
  // This is a cross-process lock which may be recursively
  // locked by the same thread.
  // TODO(port):
  // WARNING: on POSIX the lock only works across processes, not
  // across threads.  2 threads in the same process can both grab the
  // lock at the same time.  There are several solutions for this
  // (futex, lockf+anon_semaphore) but none are both clean and common
  // across Mac and Linux.
  void Lock();

#if defined(OS_WIN)
  // A Lock() implementation with a timeout that also allows setting
  // security attributes on the mutex. sec_attr may be NULL.
  // Returns true if the Lock() has been acquired, false if the timeout was
  // reached.
  bool Lock(uint32 timeout_ms, SECURITY_ATTRIBUTES* sec_attr);
#endif

  // Releases the shared memory lock.
  void Unlock();

 private:
#if defined(OS_POSIX)
  bool PrepareMapFile(FILE *fp);
  bool FilePathForMemoryName(const std::string& mem_name, FilePath* path);
  void LockOrUnlockCommon(int function);
#endif
  bool ShareToProcessCommon(ProcessHandle process,
                            SharedMemoryHandle* new_handle,
                            bool close_self);

#if defined(OS_WIN)
  std::wstring       name_;
  HANDLE             mapped_file_;
#elif defined(OS_POSIX)
  int                mapped_file_;
  uint32             mapped_size_;
  ino_t              inode_;
#endif
  void*              memory_;
  bool               read_only_;
  uint32             created_size_;
#if !defined(OS_POSIX)
  SharedMemoryLock   lock_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

// A helper class that acquires the shared memory lock while
// the SharedMemoryAutoLock is in scope.
class SharedMemoryAutoLock {
 public:
  explicit SharedMemoryAutoLock(SharedMemory* shared_memory)
      : shared_memory_(shared_memory) {
    shared_memory_->Lock();
  }

  ~SharedMemoryAutoLock() {
    shared_memory_->Unlock();
  }

 private:
  SharedMemory* shared_memory_;
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryAutoLock);
};

}  // namespace base

#endif  // BASE_SHARED_MEMORY_H_
