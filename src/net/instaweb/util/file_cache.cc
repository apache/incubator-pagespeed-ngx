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

#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// TODO(lsong): Remove the MessageHandler arg. They are not used.
// TODO(lsong): Need to create an LRU mechanism to manage the cache.
FileCache::FileCache(const std::string& path, FileSystem* file_system,
                     FilenameEncoder* filename_encoder)
    : path_(path),
      file_system_(file_system),
      filename_encoder_(filename_encoder) {
}

FileCache::~FileCache() {
}

bool FileCache::Get(const std::string& key, SharedString* value) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return false;
  }
  std::string* buffer = value->get();
  if (!file_system_->ReadFile(filename.c_str(), buffer, &message_handler_)) {
    return false;
  }
  return true;
}

void FileCache::Put(const std::string& key, SharedString* value) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  const std::string& buffer = **value;
  // TODO(jmarantz): write as temp file
  file_system_->WriteFile(filename.c_str(), buffer, &message_handler_);
}

void FileCache::Delete(const std::string& key) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  file_system_->RemoveFile(filename.c_str(), &message_handler_);
  return;
}

bool FileCache::EncodeFilename(const std::string& key,
                               std::string* filename) {
  filename_encoder_->Encode(path_, key, filename);
  return true;
}

CacheInterface::KeyState FileCache::Query(const std::string& key) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return CacheInterface::kNotFound;
  }
  if (file_system_->Exists(filename.c_str(), &message_handler_).is_true()) {
    return CacheInterface::kAvailable;
  }
  return CacheInterface::kNotFound;
}

}  // namespace net_instaweb
