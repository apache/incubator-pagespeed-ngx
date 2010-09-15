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

#include <vector>
#include <queue>
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {  // For structs used only in Clean().

class CacheFileInfo {
 public:
  CacheFileInfo(int64 size, int64 atime, const std::string& name)
      : size_(size), atime_(atime), name_(name) {}
  int64 size_;
  int64 atime_;
  std::string name_;
 private:
  DISALLOW_COPY_AND_ASSIGN(CacheFileInfo);
};

struct CompareByAtime {
 public:
  bool operator()(const CacheFileInfo* one,
                  const CacheFileInfo* two) const {
    return one->atime_ < two->atime_;
  }
};

}  // namespace for structs used only in Clean().

FileCache::FileCache(const std::string& path, FileSystem* file_system,
                     FilenameEncoder* filename_encoder,
                     MessageHandler* handler)
    : path_(path),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      message_handler_(handler) {
}

FileCache::~FileCache() {
}

bool FileCache::Get(const std::string& key, SharedString* value) {
  std::string filename;
  bool ret = EncodeFilename(key, &filename);
  if (ret) {
    std::string* buffer = value->get();

    // Suppress read errors.  Note that we want to show Write errors,
    // as they likely indicate a permissions or disk-space problem
    // which is best not eaten.  It's cheap enough to construct
    // a NullMessageHandler on the stack when we want one.
    NullMessageHandler null_handler;
    ret = file_system_->ReadFile(filename.c_str(), buffer, &null_handler);
  }
  return ret;
}

void FileCache::Put(const std::string& key, SharedString* value) {
  std::string filename;
  if (EncodeFilename(key, &filename)) {
    const std::string& buffer = **value;
    std::string temp_filename;
    if (file_system_->WriteTempFile(filename.c_str(), buffer,
                                    &temp_filename, message_handler_)) {
      file_system_->RenameFile(temp_filename.c_str(), filename.c_str(),
                               message_handler_);
    }
  }
}

void FileCache::Delete(const std::string& key) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  file_system_->RemoveFile(filename.c_str(), message_handler_);
  return;
}

bool FileCache::EncodeFilename(const std::string& key,
                               std::string* filename) {
  std::string prefix = path_;
  // TODO(abliss): unify and make explicit everyone's assumptions
  // about trailing slashes.
  EnsureEndsInSlash(&prefix);
  filename_encoder_->Encode(prefix, key, filename);
  return true;
}

CacheInterface::KeyState FileCache::Query(const std::string& key) {
  std::string filename;
  if (!EncodeFilename(key, &filename)) {
    return CacheInterface::kNotFound;
  }
  NullMessageHandler null_handler;
  if (file_system_->Exists(filename.c_str(), &null_handler).is_true()) {
    return CacheInterface::kAvailable;
  }
  return CacheInterface::kNotFound;
}

bool FileCache::Clean(int64 target_size) {
  StringVector files;
  int64 file_size;
  int64 file_atime;
  int64 total_size = 0;
  if (!file_system_->RecursiveDirSize(path_, &total_size, message_handler_)) {
    return false;
  }

  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  if (total_size < ((target_size * 5) / 4)) {
    return true;
  }

  bool everything_ok = true;
  everything_ok &= file_system_->ListContents(path_, &files, message_handler_);

  // We will now iterate over the entire directory and its children,
  // keeping a heap of files to be deleted.  Our goal is to delete the
  // oldest set of files that sum to enough space to bring us below
  // our target.
  std::priority_queue<CacheFileInfo*, std::vector<CacheFileInfo*>,
      CompareByAtime> heap;
  int64 total_heap_size = 0;
  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  int64 target_heap_size = total_size - ((target_size * 3 / 4));

  std::string prefix = path_;
  EnsureEndsInSlash(&prefix);
  for (size_t i = 0; i < files.size(); i++) {
    std::string file_name = files[i];
    BoolOrError isDir = file_system_->IsDir(file_name.c_str(),
                                            message_handler_);
    if (isDir.is_error()) {
      return false;
    } else if (isDir.is_true()) {
      // add files in this directory to the end of the vector, to be
      // examined later.
      everything_ok &= file_system_->ListContents(file_name, &files,
                                                  message_handler_);
    } else {
      everything_ok &= file_system_->Size(file_name, &file_size,
                                          message_handler_);
      everything_ok &= file_system_->Atime(file_name, &file_atime,
                                           message_handler_);
      // If our heap is still too small; add everything in.
      // Otherwise, add the file in only if it's older than the newest
      // thing in the heap.
      if ((total_heap_size < target_heap_size) ||
          (file_atime < heap.top()->atime_)) {
        CacheFileInfo* info =
            new CacheFileInfo(file_size, file_atime, file_name);
        heap.push(info);
        total_heap_size += file_size;
        // Now remove new things from the heap which are not needed
        // to keep the heap size over its target size.
        while (total_heap_size - heap.top()->size_ > target_heap_size) {
          total_heap_size -= heap.top()->size_;
          delete heap.top();
          heap.pop();
        }
      }
    }
  }
  for (size_t i = heap.size(); i > 0; i--) {
    everything_ok &= file_system_->RemoveFile(heap.top()->name_.c_str(),
                                              message_handler_);
    delete heap.top();
    heap.pop();
  }
  return everything_ok;
}

}  // namespace net_instaweb
