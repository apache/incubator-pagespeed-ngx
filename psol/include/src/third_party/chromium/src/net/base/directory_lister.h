// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DIRECTORY_LISTER_H_
#define NET_BASE_DIRECTORY_LISTER_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_api.h"

namespace net {

//
// This class provides an API for listing the contents of a directory on the
// filesystem asynchronously.  It spawns a background thread, and enumerates
// the specified directory on that thread.  It marshalls WIN32_FIND_DATA
// structs over to the main application thread.  The consumer of this class
// is insulated from any of the multi-threading details.
//
class NET_API DirectoryLister  {
 public:
  // Represents one file found.
  struct DirectoryListerData {
    file_util::FileEnumerator::FindInfo info;
    FilePath path;
  };

  // Implement this class to receive directory entries.
  class DirectoryListerDelegate {
   public:
    // Called for each file found by the lister.
    virtual void OnListFile(const DirectoryListerData& data) = 0;

    // Called when the listing is complete.
    virtual void OnListDone(int error) = 0;

   protected:
    virtual ~DirectoryListerDelegate() {}
  };

  // Sort options
  // ALPHA_DIRS_FIRST is the default sort :
  //   directories first in name order, then files by name order
  // FULL_PATH sorts by paths as strings, ignoring files v. directories
  // DATE sorts by last modified date
  enum SortType {
    NO_SORT,
    DATE,
    ALPHA_DIRS_FIRST,
    FULL_PATH
  };

  DirectoryLister(const FilePath& dir,
                  DirectoryListerDelegate* delegate);

  DirectoryLister(const FilePath& dir,
                  bool recursive,
                  SortType sort,
                  DirectoryListerDelegate* delegate);

  // Will invoke Cancel().
  ~DirectoryLister();

  // Call this method to start the directory enumeration thread.
  bool Start();

  // Call this method to asynchronously stop directory enumeration.  The
  // delegate will not be called back.
  void Cancel();

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(const FilePath& dir,
         bool recursive,
         SortType sort,
         DirectoryLister* lister);

    bool Start();

    void Cancel();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    class DataEvent;

    ~Core();

    // Runs on a WorkerPool thread.
    void StartInternal();

    void OnReceivedData(const DirectoryListerData* data, int count);
    void OnDone(int error);

    FilePath dir_;
    bool recursive_;
    SortType sort_;
    scoped_refptr<base::MessageLoopProxy> origin_loop_;

    // |lister_| gets set to NULL when canceled.
    DirectoryLister* lister_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  void OnReceivedData(const DirectoryListerData& data);
  void OnDone(int error);

  const scoped_refptr<Core> core_;
  DirectoryListerDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryLister);
};

}  // namespace net

#endif  // NET_BASE_DIRECTORY_LISTER_H_
