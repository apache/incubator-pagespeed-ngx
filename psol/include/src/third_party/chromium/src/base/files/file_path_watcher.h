// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a file or directory for changes.

#ifndef BASE_FILES_FILE_PATH_WATCHER_H_
#define BASE_FILES_FILE_PATH_WATCHER_H_
#pragma once

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

namespace base {
namespace files {

// This class lets you register interest in changes on a FilePath.
// The delegate will get called whenever the file or directory referenced by the
// FilePath is changed, including created or deleted. Due to limitations in the
// underlying OS APIs, FilePathWatcher has slightly different semantics on OS X
// than on Windows or Linux. FilePathWatcher on Linux and Windows will detect
// modifications to files in a watched directory. FilePathWatcher on Mac will
// detect the creation and deletion of files in a watched directory, but will
// not detect modifications to those files. See file_path_watcher_mac.cc for
// details.
class BASE_API FilePathWatcher {
 public:
  // Declares the callback client code implements to receive notifications. Note
  // that implementations of this interface should not keep a reference to the
  // corresponding FileWatcher object to prevent a reference cycle.
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    virtual ~Delegate() {}
    virtual void OnFilePathChanged(const FilePath& path) = 0;
    // Called when platform specific code detected an error. The watcher will
    // not call OnFilePathChanged for future changes.
    virtual void OnFilePathError(const FilePath& path) {}
  };

  FilePathWatcher();
  ~FilePathWatcher();

  // Register interest in any changes on |path|. OnPathChanged will be called
  // back for each change. Returns true on success.
  // OnFilePathChanged() will be called on the same thread as Watch() is called,
  // which should have a MessageLoop of TYPE_IO.
  bool Watch(const FilePath& path, Delegate* delegate) WARN_UNUSED_RESULT;

  class PlatformDelegate;

  // A custom Task that always cleans up the PlatformDelegate, either when
  // executed or when deleted without having been executed at all, as can
  // happen during shutdown.
  class CancelTask : public Task {
   public:
    CancelTask(PlatformDelegate* delegate): delegate_(delegate) {}
    virtual ~CancelTask() {
      delegate_->CancelOnMessageLoopThread();
    }

    virtual void Run() {
      delegate_->CancelOnMessageLoopThread();
    }
   private:
    scoped_refptr<PlatformDelegate> delegate_;

    DISALLOW_COPY_AND_ASSIGN(CancelTask);
  };

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate : public base::RefCountedThreadSafe<PlatformDelegate> {
   public:
    PlatformDelegate();

    // Start watching for the given |path| and notify |delegate| about changes.
    virtual bool Watch(const FilePath& path,
                       Delegate* delegate) WARN_UNUSED_RESULT = 0;

    // Stop watching. This is called from FilePathWatcher's dtor in order to
    // allow to shut down properly while the object is still alive.
    // It can be called from any thread.
    virtual void Cancel() = 0;

   protected:
    virtual ~PlatformDelegate();

    // Stop watching. This is only called on the thread of the appropriate
    // message loop. Since it can also be called more than once, it should
    // check |is_cancelled()| to avoid duplicate work.
    virtual void CancelOnMessageLoopThread() = 0;

    scoped_refptr<base::MessageLoopProxy> message_loop() const {
      return message_loop_;
    }

    void set_message_loop(base::MessageLoopProxy* loop) {
      message_loop_ = loop;
    }

    // Must be called before the PlatformDelegate is deleted.
    void set_cancelled() {
      cancelled_ = true;
    }

    bool is_cancelled() const {
      return cancelled_;
    }

   private:
    friend class base::RefCountedThreadSafe<PlatformDelegate>;
    friend class CancelTask;

    scoped_refptr<base::MessageLoopProxy> message_loop_;
    bool cancelled_;
  };

 private:
  scoped_refptr<PlatformDelegate> impl_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcher);
};

}  // namespace files
}  // namespace base

#endif  // BASE_FILES_FILE_PATH_WATCHER_H_
