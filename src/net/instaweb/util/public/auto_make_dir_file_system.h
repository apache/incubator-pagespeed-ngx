/**
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_AUTO_MAKE_DIR_FILE_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_AUTO_MAKE_DIR_FILE_SYSTEM_H_

#include "net/instaweb/util/public/file_system.h"

namespace net_instaweb {

// Augments a FileSystem to automatically create directories when writing files.
class AutoMakeDirFileSystem : public FileSystem {
 public:
  explicit AutoMakeDirFileSystem(FileSystem* base_file_system)
      : base_file_system_(base_file_system) {}

  virtual OutputFile* OpenOutputFile(const char* filename,
                                     MessageHandler* handler);
  virtual OutputFile* OpenTempFile(const StringPiece& prefix_name,
                                   MessageHandler* handle);
  virtual bool RenameFile(const char* old_filename, const char* new_filename,
                          MessageHandler* handler);

  // All of these just pass-through.
  virtual InputFile* OpenInputFile(const char* filename,
                                   MessageHandler* handler) {
    return base_file_system_->OpenInputFile(filename, handler);
  }
  virtual bool RemoveFile(const char* filename, MessageHandler* handler) {
    return base_file_system_->RemoveFile(filename, handler);
  }
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler) {
    return base_file_system_->MakeDir(directory_path, handler);
  }
  virtual BoolOrError Exists(const char* path, MessageHandler* handler) {
    return base_file_system_->Exists(path, handler);
  }
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler) {
    return base_file_system_->IsDir(path, handler);
  }

 private:
  void SetupFileDir(const StringPiece& filename, MessageHandler* handler);

  FileSystem* base_file_system_;

  DISALLOW_COPY_AND_ASSIGN(AutoMakeDirFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_AUTO_MAKE_DIR_FILE_SYSTEM_H_
