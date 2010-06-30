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

#include "net/instaweb/util/public/auto_make_dir_file_system.h"

#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

FileSystem::OutputFile* AutoMakeDirFileSystem::OpenOutputFile(
    const char* filename, MessageHandler* handler) {
  SetupFileDir(filename, handler);
  return base_file_system_->OpenOutputFile(filename, handler);
}

FileSystem::OutputFile* AutoMakeDirFileSystem::OpenTempFile(
    const StringPiece& prefix, MessageHandler* handler) {
  SetupFileDir(prefix, handler);
  return base_file_system_->OpenTempFile(prefix, handler);
}

bool AutoMakeDirFileSystem::RenameFile(
    const char* old_filename, const char* new_filename,
    MessageHandler* handler) {
  SetupFileDir(new_filename, handler);
  return base_file_system_->RenameFile(old_filename, new_filename, handler);
}

// Try to make directories to store file.
void AutoMakeDirFileSystem::SetupFileDir(const StringPiece& filename,
                                         MessageHandler* handler) {
  size_t last_slash = filename.rfind('/');
  if (last_slash != StringPiece::npos) {
    StringPiece directory_name = filename.substr(0, last_slash);
    if (!RecursivelyMakeDir(directory_name, handler)) {
      // TODO(sligocki): Specify where dir creation failed?
      handler->Message(kError, "Could not create directories for file %s",
                       filename.as_string().c_str());
    }
  }
}

}  // namespace net_instaweb
