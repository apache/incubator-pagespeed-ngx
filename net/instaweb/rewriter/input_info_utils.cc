/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//

#include "net/instaweb/rewriter/public/input_info_utils.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {
namespace input_info_utils {

namespace {

bool MatchesFileSystemMetadataCacheEntry(
    const InputInfo& input_info,
    const InputInfo& fsmdc_info,
    int64 mtime_ms) {
  return (fsmdc_info.has_last_modified_time_ms() &&
          fsmdc_info.has_input_content_hash() &&
          fsmdc_info.last_modified_time_ms() == mtime_ms &&
          fsmdc_info.input_content_hash() == input_info.input_content_hash());
}

// Checks if the stat() data about the input_info's file matches that in the
// filesystem metadata cache; it needs to be for the input to be "valid".
bool IsFilesystemMetadataCacheCurrent(CacheInterface* fsmdc,
                                      const GoogleString& file_key,
                                      const InputInfo& input_info,
                                      int64 mtime_ms) {
  // Get the filesystem metadata cache (FSMDC) entry for the filename.
  // If we found an entry,
  //   Extract the FSMDC timestamp and contents hash.
  //   If the FSMDC timestamp == the file's current timestamp,
  //     (the FSMDC contents hash is valid/current/correct)
  //     If the FSMDC content hash == the metadata cache's content hash,
  //       The metadata cache's entry is valid so its input_info is valid.
  //     Else
  //       Return false as the metadata cache's entry is not valid as
  //       someone has changed it on us.
  //   Else
  //     Return false as our FSMDC entry is out of date so we can't
  //     tell if the metadata cache's input_info is valid.
  // Else
  //   Return false as we can't tell if the metadata cache's input_info is
  //   valid.
  CacheInterface::SynchronousCallback callback;
  fsmdc->Get(file_key, &callback);
  DCHECK(callback.called());
  if (callback.state() == CacheInterface::kAvailable) {
    StringPiece val_str = callback.value().Value();
    ArrayInputStream input(val_str.data(), val_str.size());
    InputInfo fsmdc_info;
    if (fsmdc_info.ParseFromZeroCopyStream(&input)) {
      // We have a filesystem metadata cache entry: if its timestamp equals
      // the file's, and its contents hash equals the metadata caches's, then
      // the input is valid.
      return MatchesFileSystemMetadataCacheEntry(
          input_info, fsmdc_info, mtime_ms);
    }
  }
  return false;
}

// Update the filesystem metadata cache with the timestamp and contents hash
// of the given input's file (which is read from disk to compute the hash).
// Returns false if the file cannot be read.
bool UpdateFilesystemMetadataCache(ServerContext* server_context,
                                    const GoogleString& file_key,
                                    const InputInfo& input_info,
                                    int64 mtime_ms,
                                    CacheInterface* fsmdc,
                                    InputInfo* fsmdc_info) {
  GoogleString contents;
  if (!server_context->file_system()->ReadFile(
          input_info.filename().c_str(), &contents,
          server_context->message_handler())) {
    return false;
  }
  GoogleString contents_hash =
      server_context->contents_hasher()->Hash(contents);
  fsmdc_info->set_type(InputInfo::FILE_BASED);
  DCHECK_LT(0, mtime_ms);
  fsmdc_info->set_last_modified_time_ms(mtime_ms);
  fsmdc_info->set_input_content_hash(contents_hash);
  GoogleString buf;
  {
    // MUST be in a block so that sstream is destructed to finalize buf.
    StringOutputStream sstream(&buf);
    fsmdc_info->SerializeToZeroCopyStream(&sstream);
  }
  fsmdc->PutSwappingString(file_key, &buf);
  return true;
}

}  // namespace

// Checks whether the given input is still unchanged.
bool IsInputValid(
    ServerContext* server_context, const RewriteOptions* options,
    bool nested_rewrite, const InputInfo& input_info,
    int64 now_ms, bool* purged, bool* stale_rewrite) {
  switch (input_info.type()) {
    case InputInfo::CACHED: {
      // It is invalid if cacheable inputs have expired or ...
      DCHECK(input_info.has_expiration_time_ms());
      if (input_info.has_url()) {
        // We do not search wildcards when validating metadata because
        // that would require N wildcard matches (not even a
        // FastWildcardGroup) per input dependency.
        if (!options->IsUrlCacheValid(input_info.url(), input_info.date_ms(),
                                      false /* search_wildcards */)) {
          *purged = true;
          return false;
        }
      }
      if (!input_info.has_expiration_time_ms()) {
        return false;
      }
      int64 ttl_ms = input_info.expiration_time_ms() - now_ms;
      if (ttl_ms > 0) {
        return true;
      } else if (
          !nested_rewrite &&
          ttl_ms + options->metadata_cache_staleness_threshold_ms() > 0) {
        *stale_rewrite = true;
        return true;
      }
      return false;
    }
    case InputInfo::FILE_BASED: {
      // ... if file-based inputs have changed.
      DCHECK(input_info.has_last_modified_time_ms() &&
              input_info.has_filename());
      if (!input_info.has_last_modified_time_ms() ||
          !input_info.has_filename()) {
        return false;
      }
      int64 mtime_sec;
      server_context->file_system()->Mtime(input_info.filename(), &mtime_sec,
                                            server_context->message_handler());
      int64 mtime_ms = mtime_sec * Timer::kSecondMs;

      CacheInterface* fsmdc = server_context->filesystem_metadata_cache();
      if (fsmdc != nullptr) {
        CHECK(fsmdc->IsBlocking());
        if (!input_info.has_input_content_hash()) {
          return false;
        }
        // Construct a host-specific key. The format is somewhat arbitrary,
        // all it needs to do is differentiate the same path on different
        // hosts. If the size of the key becomes a concern we can hash it
        // and hope.
        GoogleString file_key;
        StrAppend(&file_key, "file://", server_context->hostname(),
                  input_info.filename());
        if (IsFilesystemMetadataCacheCurrent(fsmdc, file_key, input_info,
                                              mtime_ms)) {
          return true;
        }
        InputInfo fsmdc_info;
        if (!UpdateFilesystemMetadataCache(server_context, file_key,
                                            input_info, mtime_ms, fsmdc,
                                            &fsmdc_info)) {
          return false;
        }
        // Check again now that we KNOW we have the most up-to-date data
        // in the filesystem metadata cache.
        return MatchesFileSystemMetadataCacheEntry(
            input_info, fsmdc_info, mtime_ms);
      } else {
        DCHECK_LT(0, input_info.last_modified_time_ms());
        return (mtime_ms == input_info.last_modified_time_ms());
      }
    }
    case InputInfo::ALWAYS_VALID:
      return true;
  }

  LOG(DFATAL) << "Corrupt InputInfo object !?";
  return false;
}

}  // namespace input_info_utils
}  // namespace net_instaweb
