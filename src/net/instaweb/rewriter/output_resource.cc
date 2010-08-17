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
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/output_resource.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

OutputResource::OutputResource(ResourceManager* manager,
                               const ContentType* type,
                               const StringPiece& filter_prefix,
                               const StringPiece& name)
    : Resource(manager, type),
      output_file_(NULL),
      writing_complete_(false) {
  filter_prefix.CopyToString(&filter_prefix_);
  name.CopyToString(&name_);
  if (type_ != NULL) {
    suffix_ = type_->file_extension();
  }
}

OutputResource::~OutputResource() {
}

bool OutputResource::OutputWriter::Write(
    const StringPiece& data, MessageHandler* handler) {
  hasher_->Add(data);
  bool ret = http_value_->Write(data, handler);
  ret &= FileWriter::Write(data, handler);
  return ret;
}

OutputResource::OutputWriter* OutputResource::BeginWrite(
    MessageHandler* handler) {
  value_.Clear();
  Hasher* hasher = resource_manager_->hasher();
  hasher->Reset();
  CHECK(!writing_complete_);
  CHECK(output_file_ == NULL);

  // Always write to a tempfile, so that if we get interrupted in the middle
  // we won't leave a half-baked file in the serving path.
  std::string temp_prefix = TempPrefix();
  output_file_ = resource_manager_->file_system()->OpenTempFile(
      temp_prefix.c_str(), handler);
  bool success = (output_file_ != NULL);
  if (success) {
    std::string header;
    StringWriter string_writer(&header);
    meta_data_.Write(&string_writer, handler);  // Serialize header.
    // It does not make sense to have the headers in the hash.
    // call output_file_->Write directly, rather than going through
    // OutputWriter.
    //
    // TODO(jmarantz): consider refactoring to split out the header-file
    // writing in a different way, e.g. to a separate file.
    success &= output_file_->Write(header, handler);
  }
  OutputWriter* writer = NULL;
  if (success) {
    writer = new OutputWriter(output_file_, hasher, &value_);
  }
  return writer;
}

bool OutputResource::EndWrite(OutputWriter* writer, MessageHandler* handler) {
  CHECK(!writing_complete_);
  CHECK(output_file_ != NULL);
  value_.SetHeaders(meta_data_);
  Hasher* hasher = writer->hasher();
  hasher->ComputeHash(&hash_);
  writing_complete_ = true;
  std::string temp_filename = output_file_->filename();
  FileSystem* file_system = resource_manager_->file_system();
  bool ret = file_system->Close(output_file_, handler);

  // Now that we are done writing, we can rename to the filename we
  // really want.
  if (ret) {
    ret = file_system->RenameFile(temp_filename.c_str(), filename().c_str(),
                                  handler);
  }

  // TODO(jmarantz): Consider writing to the HTTP cache as we write the
  // file, so same-process write-then-read never has to read from disk.
  // This is moderately inconvenient because HTTPCache lacks a streaming
  // Put interface.

  output_file_ = NULL;
  delete writer;
  return ret;
}

// Called by FilenameOutputResource::BeginWrite to determine how
// to start writing the tmpfile.
std::string OutputResource::TempPrefix() const {
  return StrCat(resource_manager_->filename_prefix(), "temp_");
}

std::string OutputResource::NameTail() const {
  CHECK(!hash_.empty())
      << "to compute the Resource filename or URL, we must have "
      << "completed writing, otherwise the contents hash is not known.";
  std::string separator = RewriteFilter::prefix_separator();
  return StrCat(filter_prefix_, separator, hash_, separator, name_, suffix());
}

StringPiece OutputResource::suffix() const {
  CHECK(!suffix_.empty());
  return suffix_;
}

std::string OutputResource::filename() const {
  std::string filename;
  FilenameEncoder* encoder = resource_manager_->filename_encoder();
  encoder->Encode(resource_manager_->filename_prefix(), NameTail(), &filename);
  return filename;
}

std::string OutputResource::url() const {
  return StrCat(resource_manager_->url_prefix(), NameTail());
}

void OutputResource::SetHash(const StringPiece& hash) {
  CHECK(!writing_complete_);
  CHECK(hash_.empty());
  hash.CopyToString(&hash_);
}

bool OutputResource::ReadIfCached(MessageHandler* handler) {
  if (!writing_complete_) {
    FileSystem* file_system = resource_manager_->file_system();
    FileSystem::InputFile* file = file_system->OpenInputFile(
        filename().c_str(), handler);
    if (file != NULL) {
      char buf[kStackBufferSize];
      int nread = 0, num_consumed = 0;
      // TODO(jmarantz): this logic is duplicated in util/wget_url_fetcher.cc,
      // consider a refactor to merge it.
      meta_data_.Clear();
      value_.Clear();
      while (!meta_data_.headers_complete() &&
             ((nread = file->Read(buf, sizeof(buf), handler)) != 0)) {
        num_consumed = meta_data_.ParseChunk(
            StringPiece(buf, nread), handler);
      }
      value_.SetHeaders(meta_data_);
      writing_complete_ = value_.Write(
          StringPiece(buf + num_consumed, nread - num_consumed),
          handler);
      while (writing_complete_ &&
             ((nread = file->Read(buf, sizeof(buf), handler)) != 0)) {
        writing_complete_ = value_.Write(StringPiece(buf, nread), handler);
      }
      file_system->Close(file, handler);
    }
  }
  return writing_complete_;
}

bool OutputResource::IsWritten() const {
  return writing_complete_;
}

void OutputResource::SetType(const ContentType* content_type) {
  Resource::SetType(content_type);
  set_suffix(content_type->file_extension());
}

}  // namespace net_instaweb
