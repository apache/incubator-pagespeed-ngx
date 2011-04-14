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

// Author: jmarantz@google.com (Joshua Marantz)
//     and sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_

#include <map>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

class GURL;

namespace net_instaweb {

class CacheInterface;
class ContentType;
class DomainLawyer;
class FileSystem;
class FilenameEncoder;
class HTTPCache;
class HTTPValue;
class Hasher;
class MessageHandler;
class ResponseHeaders;
class NamedLockManager;
class OutputResource;
class ResourceNamer;
class RewriteOptions;
class Statistics;
class UrlAsyncFetcher;
class Variable;
class Writer;

// TODO(jmarantz): Rename this class to ServerContext, as it no longer
// contains much logic about resources -- that's been moved to RewriteDriver,
// which should be renamed RequestContext.
class ResourceManager {
 public:
  enum BlockingBehavior { kNeverBlock, kMayBlock };

  // This value is a shared constant so that it can also be used in
  // the Apache-specific code that repairs our caching headers downstream
  // of mod_headers.
  static const char kResourceEtagValue[];
  static const char kCacheKeyResourceNamePrefix[];

  ResourceManager(const StringPiece& file_prefix,
                  FileSystem* file_system,
                  FilenameEncoder* filename_encoder,
                  UrlAsyncFetcher* url_async_fetcher,
                  Hasher* hasher,
                  HTTPCache* http_cache,
                  CacheInterface* metadata_cache,
                  NamedLockManager* lock_manager,
                  MessageHandler* handler);
  ~ResourceManager();

  // Initialize statistics gathering.
  static void Initialize(Statistics* statistics);

  // Set up a basic header for a given content_type.
  // If content_type is null, the Content-Type is omitted.
  void SetDefaultHeaders(const ContentType* content_type,
                         ResponseHeaders* header) const;

  // Changes the content type of a pre-initialized header.
  void SetContentType(const ContentType* content_type, ResponseHeaders* header);

  StringPiece filename_prefix() const { return file_prefix_; }
  void set_filename_prefix(const StringPiece& file_prefix);
  Statistics* statistics() const { return statistics_; }
  void set_statistics(Statistics* s) {
    statistics_ = s;
    resource_url_domain_rejections_ = NULL;  // Lazily initialized.
  }
  void set_relative_path(bool x) { relative_path_ = x; }
  NamedLockManager* lock_manager() const { return lock_manager_; }

  // Writes the specified contents into the output resource, retaining
  // both a name->filename map and the filename->contents map.
  //
  // TODO(jmarantz): add last_modified arg.
  bool Write(HttpStatus::Code status_code,
             const StringPiece& contents, OutputResource* output,
             int64 origin_expire_time_ms, MessageHandler* handler);

  // Writes out a note that constructing given output resource is
  // not beneficial, and hence should not be attempted until origin's expiration
  // If your filter uses this, it should look at the ->optimizable() property
  // of resources when transforming
  void WriteUnoptimizable(OutputResource* output,
                          int64 origin_expire_time_ms, MessageHandler* handler);

  // Writes out a cache entry telling us how to get to the processed version
  // (output) of some resource given the original source URL and summary of the
  // processing done, such as the filter code and any custom information
  // stored by the filter which are all packed inside the ResourceNamer.
  // This entry expires as soon as the origin does. If no optimization
  // was possible, it records that fact.
  void CacheComputedResourceMapping(OutputResource* output,
                                    int64 origin_expire_time_ms,
                                    MessageHandler* handler);

  // Returns true if the resource with given date and TTL is going to expire
  // shortly and should hence be proactively re-fetched.
  bool IsImminentlyExpiring(int64 start_date_ms, int64 expire_ms) const;

  // TODO(jmarantz): check thread safety in Apache.
  Hasher* hasher() const { return hasher_; }
  // This setter should probably only be used in testing.
  void set_hasher(Hasher* hasher) { hasher_ = hasher; }

  FileSystem* file_system() { return file_system_; }
  FilenameEncoder* filename_encoder() const { return filename_encoder_; }
  UrlAsyncFetcher* url_async_fetcher() { return url_async_fetcher_; }
  void set_url_async_fetcher(UrlAsyncFetcher* fetcher) {
    url_async_fetcher_ = fetcher;
  }
  Timer* timer() const { return http_cache_->timer(); }
  HTTPCache* http_cache() { return http_cache_; }

  // Cache for small non-HTTP objects.
  //
  // Note that this might share namespace with the HTTP cache, so make sure
  // your key names do not start with http://.
  CacheInterface* metadata_cache() { return metadata_cache_; }

  // Whether or not resources should hit the filesystem.
  bool store_outputs_in_file_system() { return store_outputs_in_file_system_; }
  void set_store_outputs_in_file_system(bool store) {
    store_outputs_in_file_system_ = store;
  }

  void RefreshIfImminentlyExpiring(Resource* resource,
                                   MessageHandler* handler) const;

  // This method is here because it updates a thread-safe Statistic which
  // can be shared between multiple requests.
  void IncrementResourceUrlDomainRejections();

  MessageHandler* message_handler() const { return message_handler_; }

 private:
  GoogleString file_prefix_;
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlAsyncFetcher* url_async_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;
  Variable* resource_url_domain_rejections_;
  HTTPCache* http_cache_;
  CacheInterface* metadata_cache_;
  bool relative_path_;
  bool store_outputs_in_file_system_;
  NamedLockManager* lock_manager_;
  GoogleString max_age_string_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
