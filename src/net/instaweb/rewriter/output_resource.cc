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
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

OutputResource::OutputResource(ResourceManager* manager,
                               const StringPiece& resolved_base,
                               const ResourceNamer& full_name,
                               const ContentType* type)
    : Resource(manager, type),
      output_file_(NULL),
      writing_complete_(false),
      generated_(false),
      resolved_base_(resolved_base.data(), resolved_base.size()),
      full_name_() {
  full_name_.CopyFrom(full_name);
  if (type == NULL) {
    std::string ext_with_dot = StrCat(".", full_name.ext());
    type_ = NameExtensionToContentType(ext_with_dot);
  } else {
    // This if + check used to be a 1-liner, but it was failing and this
    // yields debuggable output.
    // TODO(jmaessen): The addition of 1 below avoids the leading ".";
    // make this convention consistent and fix all code.
    CHECK_EQ((type->file_extension() + 1), full_name.ext());
  }
}

OutputResource::~OutputResource() {
}

bool OutputResource::OutputWriter::Write(const StringPiece& data,
                                         MessageHandler* handler) {
  bool ret = http_value_->Write(data, handler);
  if (file_writer_.get() != NULL) {
    ret &= file_writer_->Write(data, handler);
  }
  return ret;
}

OutputResource::OutputWriter* OutputResource::BeginWrite(
    MessageHandler* handler) {
  value_.Clear();
  full_name_.ClearHash();
  CHECK(!writing_complete_);
  CHECK(output_file_ == NULL);
  if (resource_manager_->store_outputs_in_file_system()) {
    FileSystem* file_system = resource_manager_->file_system();
    // Always write to a tempfile, so that if we get interrupted in the middle
    // we won't leave a half-baked file in the serving path.
    std::string temp_prefix = TempPrefix();
    output_file_ = file_system->OpenTempFile(
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
      writer = new OutputWriter(output_file_, &value_);
    }
    return writer;
  } else {
    return new OutputWriter(NULL, &value_);
  };
}

bool OutputResource::EndWrite(OutputWriter* writer, MessageHandler* handler) {
  CHECK(!writing_complete_);
  value_.SetHeaders(meta_data_);
  Hasher* hasher = resource_manager_->hasher();
  full_name_.set_hash(hasher->Hash(contents()));
  writing_complete_ = true;
  if (output_file_ == NULL) {
    return true;
  } else {
    FileSystem* file_system = resource_manager_->file_system();
    CHECK(file_system != NULL);
    std::string temp_filename = output_file_->filename();
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
}

// Called by FilenameOutputResource::BeginWrite to determine how
// to start writing the tmpfile.
std::string OutputResource::TempPrefix() const {
  return StrCat(resource_manager_->filename_prefix(), "temp_");
}

StringPiece OutputResource::suffix() const {
  CHECK(type_ != NULL);
  return type_->file_extension();
}

void OutputResource::set_suffix(const StringPiece& ext) {
  type_ = NameExtensionToContentType(ext);
  if (type_ != NULL) {
    // TODO(jmaessen): The addition of 1 below avoids the leading ".";
    // make this convention consistent and fix all code.
    full_name_.set_ext(type_->file_extension() + 1);
  } else {
    full_name_.set_ext(ext.substr(1));
  }
}

std::string OutputResource::filename() const {
  std::string filename;
  resource_manager_->filename_encoder()->Encode(
      resource_manager_->filename_prefix(), url(), &filename);
  return filename;
}

std::string OutputResource::name_key() const {
  std::string id_name = full_name_.EncodeIdName();
  std::string result;
  CHECK(!resolved_base_.empty());  // Corresponding path in url() is dead code
  // TODO(jmaessen): Fix when we're consistent; see url() below.
  if (resolved_base_[resolved_base_.size() - 1] == '/') {
    result = StrCat(resolved_base_, id_name);
  } else {
    result = StrCat(resolved_base_, "/", id_name);
  }
  return result;
}

std::string OutputResource::hash_ext() const {
  return full_name_.EncodeHashExt();
}

// TODO(jmarantz): change the name to reflect the fact that it is not
// just an accessor now.
std::string OutputResource::url() const {
  std::string encoded = full_name_.Encode();
  if (resolved_base_.empty()) {
    // Resolves sharding, shoddily.
    std::string base =
        resource_manager_->UrlPrefixFor(full_name_);
    encoded = StrCat(base, encoded);
  } else {
    // TODO(jmaessen): this is a band aid compensating for the fact that we
    // aren't consistent about trailing / in resolved_base.  If we believe we're
    // always getting these from GoogleUrl::AllExceptLeaf they ought to lack the
    // trailing /.  But partnership->ResolvedBase() appears to have a trailing /
    // in some circumstances (cf css_combine for examples that go wrong).
    if (resolved_base_[resolved_base_.size() - 1] == '/') {
      encoded = StrCat(resolved_base_, encoded);
    } else {
      encoded = StrCat(resolved_base_, "/", encoded);
    }
  }
  return encoded;
}

void OutputResource::SetHash(const StringPiece& hash) {
  CHECK(!writing_complete_);
  CHECK(!has_hash());
  full_name_.set_hash(hash);
}

bool OutputResource::ReadIfCached(MessageHandler* handler) {
  if (!writing_complete_ && resource_manager_->store_outputs_in_file_system()) {
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
  // TODO(jmaessen): The addition of 1 below avoids the leading ".";
  // make this convention consistent and fix all code.
  full_name_.set_ext(content_type->file_extension() + 1);
}

}  // namespace net_instaweb
