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

#include "base/logging.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

namespace {

const char kLockSuffix[] = ".outputlock";

// Prefix we use to distinguish keys used by filters
const char kCustomKeyPrefix[] = "X-ModPagespeedCustom-";

// OutputResource::{Fetch,Save}Cached encodes the state
// of the optimizable bit via presence of this header
const char kCacheUnoptimizableHeader[] = "X-ModPagespeed-Unoptimizable";

}  // namespace

OutputResource::CachedResult::CachedResult() : frozen_(false),
                                               optimizable_(true),
                                               origin_expiration_time_ms_(0) {}

void OutputResource::CachedResult::SetRemembered(const char* key,
                                                 const std::string& val) {
  DCHECK(!frozen_) << "Any custom metadata must be set before "
                      "ResourceManager::Write* is called";
  std::string full_key = StrCat(StringPiece(kCustomKeyPrefix), key);
  headers_.RemoveAll(full_key.c_str());
  headers_.Add(full_key, val);
}

bool OutputResource::CachedResult::Remembered(const char* key,
                                              std::string* out) const {
  std::string full_key = StrCat(StringPiece(kCustomKeyPrefix), key);
  CharStarVector vals;
  if (headers_.Lookup(full_key.c_str(), &vals) && vals.size() == 1) {
    *out = vals[0];
    return true;
  }
  return false;
}

void OutputResource::CachedResult::SetRememberedInt64(const char* key,
                                                      int64 val) {
  SetRemembered(key, Integer64ToString(val));
}

bool OutputResource::CachedResult::RememberedInt64(const char* key,
                                                   int64* out) {
  std::string out_str;
  return Remembered(key, &out_str) && StringToInt64(out_str, out);
}

void OutputResource::CachedResult::SetRememberedInt(const char* key, int val) {
  SetRemembered(key, IntegerToString(val));
}

bool OutputResource::CachedResult::RememberedInt(const char* key, int* out) {
  std::string out_str;
  return Remembered(key, &out_str) && StringToInt(out_str, out);
}

OutputResource::OutputResource(ResourceManager* manager,
                               const StringPiece& resolved_base,
                               const ResourceNamer& full_name,
                               const ContentType* type,
                               const RewriteOptions* options)
    : Resource(manager, type),
      output_file_(NULL),
      writing_complete_(false),
      generated_(false),
      resolved_base_(resolved_base.data(), resolved_base.size()),
      rewrite_options_(options) {
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
  CHECK(EndsInSlash(resolved_base)) << "resolved_base must end in a slash.";
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

std::string OutputResource::hash_ext() const {
  return full_name_.EncodeHashExt();
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
    GURL gurl = GoogleUrl::Create(resolved_base_);
    std::string domain = StrCat(GoogleUrl::Origin(gurl), "/");
    if (lawyer->ShardDomain(domain, int_hash, &shard)) {
      // The Path has a leading "/", and shard has a trailing "/".  So
      // we need to perform some StringPiece substring arithmetic to
      // make them all fit together.  Note that we could have used
      // string's substr method but that would have made another temp
      // copy, which seems like a waste.
      shard_path = StrCat(shard, StringPiece(GoogleUrl::Path(gurl)).substr(1));
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
                                      MessageHandler* handler) const {
  HTTPCache* http_cache = resource_manager()->http_cache();
  CachedResult* cached = cached_result_.get();
  CHECK(cached != NULL);
  cached->set_frozen(true);

  int64 delta_ms = cached->origin_expiration_time_ms() -
                       http_cache->timer()->NowMs();
  int64 delta_sec = delta_ms / Timer::kSecondMs;
  if ((delta_sec > 0) || http_cache->force_caching()) {
    ResponseHeaders* meta_data = &cached->headers_;
    resource_manager()->SetDefaultHeaders(type(), meta_data);
    std::string cache_control = StringPrintf(
        "max-age=%ld",
        static_cast<long>(delta_sec));  // NOLINT
    meta_data->RemoveAll(HttpAttributes::kCacheControl);
    meta_data->Add(HttpAttributes::kCacheControl, cache_control);
    meta_data->RemoveAll(kCacheUnoptimizableHeader);
    if (!cached->optimizable()) {
      meta_data->Add(kCacheUnoptimizableHeader, "true");
    }
    meta_data->ComputeCaching();

    std::string file_mapping;
    if (cached->optimizable()) {
      file_mapping = hash_ext();
    }
    http_cache->Put(name_key, meta_data, file_mapping, handler);
  }
}

void OutputResource::FetchCachedResult(const std::string& name_key,
                                       MessageHandler* handler) {
  HTTPCache* cache = resource_manager()->http_cache();
  cached_result_.reset();
  CachedResult* cached = EnsureCachedResultCreated();

  StringPiece hash_extension;
  HTTPValue value;
  bool ok = false;
  bool found = cache->Find(name_key, &value, &cached->headers_, handler) ==
                   HTTPCache::kFound;
  if (found && value.ExtractContents(&hash_extension)) {
    cached->set_origin_expiration_time_ms(
        cached->headers_.CacheExpirationTimeMs());
    CharStarVector dummy;
    if (cached->headers_.Lookup(kCacheUnoptimizableHeader, &dummy)) {
      cached->set_optimizable(false);
      ok = true;
    } else {
      ResourceNamer hash_ext;
      if (hash_ext.DecodeHashExt(hash_extension)) {
        SetHash(hash_ext.hash());
        // Note that the '.' must be included in the suffix
        // TODO(jmarantz): remove this from the suffix.
        set_suffix(StrCat(".", hash_ext.ext()));
        cached->set_optimizable(true);
        cached->set_url(url());
        ok = true;
      }
    }
  }

  if (!ok) {
    cached_result_.reset();
  }
}

}  // namespace net_instaweb
