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

#include "net/instaweb/rewriter/public/hash_output_resource.h"

#include <assert.h>

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

HashOutputResource::HashOutputResource(const StringPiece& url_prefix,
                                       const StringPiece& filename_prefix,
                                       const StringPiece& filter_prefix,
                                       const StringPiece& name,
                                       const StringPiece& suffix,
                                       FileSystem* file_system,
                                       FilenameEncoder* filename_encoder,
                                       Hasher* hasher)
    : file_system_(file_system),
      output_file_(NULL),
      writing_complete_(false),
      filename_encoder_(filename_encoder),
      hasher_(hasher) {
  url_prefix.CopyToString(&url_prefix_);
  filename_prefix.CopyToString(&filename_prefix_);
  filter_prefix.CopyToString(&filter_prefix_);
  name.CopyToString(&name_);
  suffix.CopyToString(&suffix_);
  // Note: url is empty until we write contents of file.
}

class HashOutputWriter : public FileWriter {
 public:
  HashOutputWriter(FileSystem::OutputFile* file, Hasher* hasher)
      : FileWriter(file),
        hasher_(hasher) {
  }

  virtual bool Write(const StringPiece& data, MessageHandler* handler) {
    hasher_->Add(data);
    return FileWriter::Write(data, handler);
  }

 private:
  Hasher* hasher_;
};

Writer* HashOutputResource::BeginWrite(MessageHandler* handler) {
  hasher_->Reset();
  assert(!writing_complete_);
  assert(output_file_ == NULL);

  // Always write to a tempfile, so that if we get interrupted in the middle
  // we won't leave a half-baked file in the serving path.
  std::string temp_prefix = TempPrefix();
  output_file_ = file_system_->OpenTempFile(temp_prefix.c_str(), handler);
  bool success = (output_file_ != NULL);
  if (success) {
    std::string header;
    StringWriter string_writer(&header);
    metadata_.Write(&string_writer, handler);  // Serialize header.
    // It does not make sense to have the headers in the hash.
    // call output_file_->Write directly, rather than going through
    // HashOutputWriter.
    //
    // TODO(jmarantz): consider refactoring to split out the header-file
    // writing in a different way, e.g. to a separate file.
    success &= output_file_->Write(header, handler);
  }
  HashOutputWriter* writer = NULL;
  if (success) {
    writer = new HashOutputWriter(output_file_, hasher_);
  }
  return writer;
}

// Called by FilenameOutputResource::StartWrite to determine how
// to start writing the tmpfile.
std::string HashOutputResource::TempPrefix() const {
  return StrCat(filename_prefix_, "temp_");
}

bool HashOutputResource::EndWrite(Writer* writer, MessageHandler* handler) {
  assert(!writing_complete_);
  assert(output_file_ != NULL);
  hasher_->ComputeHash(&hash_);
  std::string separator = RewriteFilter::prefix_separator();
  std::string tail = StrCat(
      filter_prefix_, separator, hash_, separator, name_, suffix_);
  url_ = StrCat(url_prefix_, tail);
  filename_encoder_->Encode(filename_prefix_, tail, &filename_);

  writing_complete_ = true;
  std::string temp_filename = output_file_->filename();
  bool ret = file_system_->Close(output_file_, handler);

  // Now that we are done writing, we can rename to the filename we
  // really want.
  if (ret) {
    ret = file_system_->RenameFile(temp_filename.c_str(), filename_.c_str(),
                                   handler);
  }

  output_file_ = NULL;
  delete writer;
  return ret;
}

StringPiece HashOutputResource::url() const {
  if (url_.empty()) {
    // Error: tried to get url before Writing file.
    assert(false);
  }
  return url_;
}

bool HashOutputResource::Read(Writer* writer, MetaData* response_headers,
                              MessageHandler* handler) const {
  assert(writing_complete_);
  bool ret = true;
  FileSystem::InputFile* file = file_system_->OpenInputFile(
      filename_.c_str(), handler);
  if (file == NULL) {
    ret = false;
  } else {
    char buf[kStackBufferSize];
    int nread = 0, num_consumed = 0;
    // TODO(jmarantz): this logic is duplicated in util/wget_url_fetcher.cc,
    // consider a refactor to merge it.
    while (!response_headers->headers_complete() &&
           ((nread = file->Read(buf, sizeof(buf), handler)) != 0)) {
      num_consumed = response_headers->ParseChunk(
          StringPiece(buf, nread), handler);
    }
    ret = writer->Write(StringPiece(buf + num_consumed, nread - num_consumed),
                        handler);
    while (ret && ((nread = file->Read(buf, sizeof(buf), handler)) != 0)) {
      ret = writer->Write(StringPiece(buf, nread), handler);
    }
    file_system_->Close(file, handler);
  }
  return ret;
}

// Resources stored in a file system are readable as soon as they are written.
// But if we were to store resources in a CDN with a 1 minute push process, then
// it's possible that IsReadable might lag IsWritten.
bool HashOutputResource::IsReadable() const {
  return writing_complete_;
}

bool HashOutputResource::IsWritten() const {
  return writing_complete_;
}

}  // namespace net_instaweb
