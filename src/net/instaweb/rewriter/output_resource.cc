/*
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

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

namespace {

const char kLockSuffix[] = ".outputlock";

// Helper to allow us to use synchronous caches synchronously even with
// asynchronous interface, until we're changed to be fully asynchronous.
class SyncCallback : public CacheInterface::Callback {
 public:
  SyncCallback() : called_(false), state_(CacheInterface::kNotFound) {
  }

  virtual void Done(CacheInterface::KeyState state) {
    called_ = true;
    state_ = state;
  }

  bool called_;
  CacheInterface::KeyState state_;
};

}  // namespace

OutputResource::OutputResource(RewriteDriver* driver,
                               const StringPiece& resolved_base,
                               const ResourceNamer& full_name,
                               const ContentType* type,
                               const RewriteOptions* options,
                               Kind kind)
    : Resource(driver, type),
      output_file_(NULL),
      writing_complete_(false),
      resolved_base_(resolved_base.data(), resolved_base.size()),
      rewrite_options_(options),
      kind_(kind) {
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
  CHECK(EndsInSlash(resolved_base)) <<
      "resolved_base must end in a slash, was: " << resolved_base;
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
    output_file_ = file_system->OpenTempFile(temp_prefix, handler);
    bool success = (output_file_ != NULL);
    if (success) {
      std::string header;
      StringWriter string_writer(&header);
      meta_data_.WriteAsHttp(&string_writer, handler);  // Serialize header.
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
  value_.SetHeaders(&meta_data_);
  Hasher* hasher = resource_manager_->hasher();
  full_name_.set_hash(hasher->Hash(contents()));
  writing_complete_ = true;
  bool ret = true;
  if (output_file_ != NULL) {
    FileSystem* file_system = resource_manager_->file_system();
    CHECK(file_system != NULL);
    std::string temp_filename = output_file_->filename();
    ret = file_system->Close(output_file_, handler);

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
  }
  if (creation_lock_.get() != NULL) {
    // We've created the data, never need to lock again.
    creation_lock_->Unlock();
    creation_lock_.reset(NULL);
  }
  return ret;
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
  result = StrCat(resolved_base_, id_name);
  return result;
}

// TODO(jmarantz): change the name to reflect the fact that it is not
// just an accessor now.
std::string OutputResource::url() const {
  std::string shard, shard_path;
  std::string encoded(full_name_.Encode());
  if (rewrite_options_ != NULL) {
    StringPiece hash = full_name_.hash();
    uint32 int_hash = HashString<CasePreserve, uint32>(
        hash.data(), hash.size());
    const DomainLawyer* lawyer = rewrite_options_->domain_lawyer();
    GoogleUrl gurl(resolved_base_);
    std::string domain = StrCat(gurl.Origin(), "/");
    if (lawyer->ShardDomain(domain, int_hash, &shard)) {
      // The Path has a leading "/", and shard has a trailing "/".  So
      // we need to perform some StringPiece substring arithmetic to
      // make them all fit together.  Note that we could have used
      // string's substr method but that would have made another temp
      // copy, which seems like a waste.
      shard_path = StrCat(shard, gurl.Path().substr(1));
    }
  }
  if (shard_path.empty()) {
    encoded = StrCat(resolved_base_, encoded);
  } else {
    encoded = StrCat(shard_path, encoded);
  }
  return encoded;
}

void OutputResource::SetHash(const StringPiece& hash) {
  CHECK(!writing_complete_);
  CHECK(!has_hash());
  full_name_.set_hash(hash);
}

bool OutputResource::Load(MessageHandler* handler) {
  if (!writing_complete_ && resource_manager_->store_outputs_in_file_system() &&
      (kind_ != kOnTheFlyResource)) {
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

      // TODO(jmarantz): convert to binary headers
      ResponseHeadersParser parser(&meta_data_);
      while (!parser.headers_complete() &&
             ((nread = file->Read(buf, sizeof(buf), handler)) != 0)) {
        num_consumed = parser.ParseChunk(StringPiece(buf, nread), handler);
      }
      value_.SetHeaders(&meta_data_);
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

bool OutputResource::LockForCreation(const ResourceManager* resource_manager,
                                     ResourceManager::BlockingBehavior block) {
  const int64 break_lock_ms = 30 * Timer::kSecondMs;
  const int64 block_lock_ms = 5 * Timer::kSecondMs;
  bool result = true;
  if (creation_lock_.get() == NULL) {
    std::string lock_name =
        StrCat(resource_manager->filename_prefix(),
               resource_manager->hasher()->Hash(name_key()),
               kLockSuffix);
    creation_lock_.reset(resource_manager->lock_manager()->
                         CreateNamedLock(lock_name));
  }
  switch (block) {
    case ResourceManager::kNeverBlock:
      // TODO(jmaessen): When caller retries properly in all cases, use
      // LockTimedWaitStealOld with a sub-second timeout to try to catch
      // rewritten data.
      result = creation_lock_->TryLockStealOld(break_lock_ms);
      break;
    case ResourceManager::kMayBlock:
      creation_lock_->LockStealOld(block_lock_ms);
      break;
  }
  return result;
}

void OutputResource::SaveCachedResult(const std::string& name_key,
                                      MessageHandler* handler) {
  CacheInterface* cache = resource_manager_->metadata_cache();
  CachedResult* cached = EnsureCachedResultCreated();
  cached->set_frozen(true);

  int64 delta_ms = cached->origin_expiration_time_ms() -
                       resource_manager_->timer()->NowMs();
  int64 delta_sec = delta_ms / Timer::kSecondMs;
  if (!cached->auto_expire()) {
    delta_sec = std::max(delta_sec, Timer::kYearMs / Timer::kSecondMs);
  }
  if ((delta_sec > 0) || resource_manager_->http_cache()->force_caching()) {
    if (cached->optimizable()) {
      cached->set_hash(full_name_.hash().as_string());
      cached->set_extension(full_name_.ext().as_string());
    }
    SharedString buf;
    {
      StringOutputStream sstream(buf.get());
      cached->SerializeToZeroCopyStream(&sstream);
      // destructor of sstream prepares *buf.get()
    }
    cache->Put(name_key, &buf);
  }
}

void OutputResource::FetchCachedResult(const std::string& name_key,
                                       MessageHandler* handler) {
  bool ok = false;
  CacheInterface* cache = resource_manager_->metadata_cache();
  cached_result_.reset();
  CachedResult* cached = EnsureCachedResultCreated();

  SyncCallback callback;
  cache->Get(name_key, &callback);
  CHECK(callback.called_) << "Async metadata caches not supported yet";

  if (callback.state_ == CacheInterface::kAvailable) {
    SharedString* val = callback.value();
    ArrayInputStream input(val->get()->data(), val->size());
    if (cached->ParseFromZeroCopyStream(&input)) {
      cached->set_frozen(false);
      if (!cached->optimizable()) {
        ok = true;
      } else if (cached->has_hash() && cached->has_extension()) {
        SetHash(cached->hash());
        // Note that the '.' must be included in the suffix
        // TODO(jmarantz): remove this from the suffix.
        set_suffix(StrCat(".", cached->extension()));
        cached->set_optimizable(true);
        cached->set_url(url());
        ok = true;
      }
    }

    // Apply auto-expire if needed & enabled
    if (ok && cached->auto_expire()) {
      if (cached->origin_expiration_time_ms() <=
          resource_manager_->timer()->NowMs()) {
        ok = false;
      }
    }
  }

  if (!ok) {
    cached_result_.reset();
  }
}

}  // namespace net_instaweb
