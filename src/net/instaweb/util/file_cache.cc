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

// Author: lsong@google.com (Libo Song)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/util/public/file_cache.h"

#include <assert.h>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

// TODO(lsong): Need to create an LRU mechanism to manage the cache.
FileCache::FileCache(const std::string& path, FileSystem* file_system,
                     MessageHandler* message_handler)
    : path_(path),
      file_system_(file_system),
      message_handler_(message_handler) {
}

FileCache::~FileCache() {
}

bool FileCache::Get(const std::string& key, Writer* writer,
                   MessageHandler* message_handler) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return false;
  }
  FileSystem::InputFile* in_file
      = file_system_->OpenInputFile(filename.c_str(), message_handler);
  if (in_file == NULL) {
    return false;
  }
  char buffer[kStackBufferSize];
  int bytes = 0;
  while ((bytes = in_file->Read(buffer, kStackBufferSize, message_handler))
          > 0 ) {
    if (!writer->Write(StringPiece(buffer, bytes), message_handler)) {
      file_system_->Close(in_file, message_handler_);
      return false;
    }
  }
  writer->Flush(message_handler);
  file_system_->Close(in_file, message_handler_);
  return true;
}

void FileCache::Put(const std::string& key, const std::string& new_value,
                    MessageHandler* message_handler) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  FileSystem::OutputFile* out_file =
      file_system_->OpenOutputFile(filename.c_str(), message_handler_);
  if (out_file == NULL) {
    return;  // Failed to open the output file.
  }

  if (!out_file->Write(new_value, message_handler_)) {
    file_system_->Close(out_file, message_handler_);
    return;  // Failed to write the file.
  }

  out_file->Flush(message_handler_);
  file_system_->Close(out_file, message_handler_);
  return;
}

void FileCache::Delete(const std::string& key,
                       MessageHandler* message_handler) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  file_system_->RemoveFile(filename.c_str(), message_handler_);
  return;
}

bool FileCache::EncodeFilename(const std::string& key,
                               std::string* filename) {
  std::string encoded_key;
  Web64Encode(key, &encoded_key);
  *filename = path_;
  filename->append("/");
  filename->append(encoded_key);
  return true;
}

// TODO(lsong): Inefficient to use open to check if the file available.
CacheInterface::KeyState FileCache::Query(const std::string& key,
                                          MessageHandler* message_handler) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return CacheInterface::kNotFound;
  }

  FileSystem::InputFile* in_file
      = file_system_->OpenInputFile(filename.c_str(), message_handler);
  if (in_file == NULL) {
    return CacheInterface::kNotFound;
  }
  file_system_->Close(in_file, message_handler);
  return CacheInterface::kAvailable;
}

}  // namespace net_instaweb
