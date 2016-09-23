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

#include "net/instaweb/rewriter/public/file_input_resource.h"

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/input_info.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace {

const int64 kTimestampUnset = 0;

}  // namespace

namespace net_instaweb {

FileInputResource::FileInputResource(const RewriteDriver* driver,
                                     const ContentType* type, StringPiece url,
                                     StringPiece filename)
    : Resource(driver, type),
      url_(url.data(), url.size()),
      filename_(filename.data(), filename.size()),
      last_modified_time_sec_(kTimestampUnset),
      max_file_size_(
          driver->options()->max_cacheable_response_content_length()),
      load_from_file_cache_ttl_ms_(
          driver->options()->load_from_file_cache_ttl_ms()),
      load_from_file_ttl_set_(
          driver->options()->load_from_file_cache_ttl_ms_was_set()) {
}

FileInputResource::~FileInputResource() {
}

// File input resources don't have expirations, we assume that the resource
// is valid as long as the FileInputResource lives.
bool FileInputResource::IsValidAndCacheable() const {
  // File is statted in RewriteContext::IsInputValid(). After which it's
  // status should be set to kOK.
  return response_headers_.status_code() == HttpStatus::kOK;
}

void FileInputResource::FillInPartitionInputInfo(
    HashHint include_content_hash, InputInfo* input) {
  CHECK(loaded());
  input->set_type(InputInfo::FILE_BASED);
  if (last_modified_time_sec_ == kTimestampUnset) {
    LOG(DFATAL) << "We should never have populated FileInputResource without "
        "a timestamp for " << filename_;

    // Resources can in theory be preloaded via HTTP cache, in which
    // case we'll have loaded() == true, but last_modified_time_sec_
    // unset.  We should be preventing this at a higher level because
    // FileInputResource::UseHttpCache returns false.  But we'll
    // defensively fill in the timestamp anyway in production.
    FileSystem* file_system = server_context_->file_system();
    if (!file_system->Mtime(filename_, &last_modified_time_sec_,
                            server_context()->message_handler())) {
      LOG(DFATAL) << "Could not get last_modified_time_ for file " << filename_;
    }
  }

  input->set_last_modified_time_ms(last_modified_time_sec_ * Timer::kSecondMs);
  input->set_filename(filename_);
  // If the file is valid and we are using a filesystem metadata cache, save
  // the hash of the file's contents for subsequent storing into it (the cache).
  if (IsValidAndCacheable() &&
      server_context_->filesystem_metadata_cache() != NULL) {
    input->set_input_content_hash(ContentsHash());
  }
}

// TODO(sligocki): Is this reasonable? People might want custom headers.
//
// For example, Content-Type is set solely by file extension and will not
// be set if the extension is unknown :/
//
// Date, Last-Modified and Cache-Control headers are set to support an
// implicit 5 min cache lifetime (for sync flow).
void FileInputResource::SetDefaultHeaders(const ContentType* content_type,
                                          ResponseHeaders* header,
                                          MessageHandler* handler) {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);
  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type == NULL) {
    handler->Message(kError, "Loaded resource with no Content-Type %s",
                     url_.c_str());
  } else {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }
  // Note(sligocki): We are setting these to get FileInputResources
  // automatically cached for 5 minutes on the sync pathway. We could
  // probably remove it once we kill the sync pathway.
  int64 cache_ttl_ms;
  if (load_from_file_ttl_set_) {
    cache_ttl_ms = load_from_file_cache_ttl_ms_;
  } else {
    cache_ttl_ms = header->implicit_cache_ttl_ms();
  }
  header->SetDateAndCaching(server_context_->timer()->NowMs(), cache_ttl_ms);
  header->SetLastModified(last_modified_time_sec_ * Timer::kSecondMs);
  header->ComputeCaching();
}

// Note: We do not save this resource to the HttpCache, so it will be
// reloaded for every request.
void FileInputResource::LoadAndCallback(
    NotCacheablePolicy not_cacheable_policy,
    const RequestContextPtr& request_context,
    AsyncCallback* callback) {
  MessageHandler* handler = server_context()->message_handler();
  if (!loaded()) {
    // Load the file from disk.  Make sure we correctly read a timestamp
    // before loading the file.  A failure (say due to EINTR) on the
    // timestamp read could leave us with populated metadata and
    // an unset timestamp.
    //
    // TODO(jmarantz): it would be much better to use fstat on the
    // same file-handle we use for reading, rather than doing two
    // distinct file lookups, which is both slower and can introduce
    // skew.
    // TODO(jefftk): Refactor the FileSystem API to allow you to Open() a handle
    // and then make a series of calls on it.  Probably caching stat responses.
    FileSystem* file_system = server_context_->file_system();
    if (file_system->Mtime(filename_, &last_modified_time_sec_, handler) &&
        last_modified_time_sec_ != kTimestampUnset &&
        file_system->ReadFile(
            filename_.c_str(), max_file_size_, &value_, handler)) {
      SetDefaultHeaders(type_, &response_headers_, handler);
      value_.SetHeaders(&response_headers_);
    } else {
      value_.Clear();
      response_headers_.Clear();
      last_modified_time_sec_ = kTimestampUnset;
    }
  }
  // If we failed to load the file above then loaded() will return false, and
  // we'll fall back to http-based loading.
  callback->Done(false /* lock_failure */, loaded());
}

}  // namespace net_instaweb
