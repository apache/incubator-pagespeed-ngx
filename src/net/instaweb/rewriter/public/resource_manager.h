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
#include "net/instaweb/util/public/basictypes.h"
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

class AbstractLock;
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

typedef RefCountedPtr<OutputResource> OutputResourcePtr;

// TODO(jmarantz): Rename this class to ServerContext, as it no longer
// contains much logic about resources -- that's been moved to RewriteDriver,
// which should be renamed RequestContext.
class ResourceManager {
 public:
  enum BlockingBehavior { kNeverBlock, kMayBlock };

  enum Kind {
    kRewrittenResource,  // derived from some input resource URL or URLs.
    kOnTheFlyResource,   // derived from some input resource URL or URLs in a
                         //   very inexpensive way --- it makes no sense to
                         //   cache the output contents.
    kOutlinedResource    // derived from page HTML.
  };

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
                  MessageHandler* handler,
                  Statistics* statistics);
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

  Variable* resource_url_domain_rejections() {
    return resource_url_domain_rejections_;
  }
  Variable* cached_output_missed_deadline() {
    return cached_output_missed_deadline_;
  }
  Variable* cached_output_hits() {
    return cached_output_hits_;
  }
  Variable* cached_output_misses() {
    return cached_output_misses_;
  }
  Variable* resource_404_count() { return resource_404_count_; }
  Variable* slurp_404_count() { return slurp_404_count_; }

  MessageHandler* message_handler() const { return message_handler_; }

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents are cached, the callback will
  // be called directly, rather than asynchronously.  The resource
  // will be passed to the callback, with its contents and headers filled in.
  void ReadAsync(Resource::AsyncCallback* callback);

  // Creates a reference-counted pointer to a new OutputResource object.
  //
  // The content type is taken from the input_resource, but can be modified
  // with SetType later if that is not correct (e.g. due to image transcoding).

  // Constructs an output resource corresponding to the specified input resource
  // and encoded using the provided encoder.  Assumes permissions checking
  // occurred when the input resource was constructed, and does not do it again.
  // To avoid if-chains, tolerates a NULL input_resource (by returning NULL).
  // TODO(jmaessen, jmarantz): Do we want to permit NULL input_resources here?
  // jmarantz has evinced a distaste.
  OutputResourcePtr CreateOutputResourceFromResource(
      const RewriteOptions* options,
      const StringPiece& filter_prefix,
      const UrlSegmentEncoder* encoder,
      const ResourceContext* data,
      const ResourcePtr& input_resource,
      Kind kind);

  // Creates an output resource where the name is provided by the rewriter.
  // The intent is to be able to derive the content from the name, for example,
  // by encoding URLs and metadata.
  //
  // This method succeeds unless the filename is too long.
  //
  // This name is prepended with path for writing hrefs, and the resulting url
  // is encoded and stored at file_prefix when working with the file system.  So
  // hrefs are:
  //    $(PATH)/$(NAME).pagespeed.$(FILTER_PREFIX).$(HASH).$(CONTENT_TYPE_EXT)
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  OutputResourcePtr CreateOutputResourceWithPath(
      const RewriteOptions* options, const StringPiece& path,
      const StringPiece& filter_prefix, const StringPiece& name,
      const ContentType* type, Kind kind);

  // Attempt to obtain a named lock.  Return true if we do so.  If the
  // object is expensive to create, this lock should be held during
  // its creation to avoid multiple rewrites happening at once.  The
  // lock will be unlocked when creation_lock is reset or destructed.
  bool LockForCreation(const GoogleString& name,
                       ResourceManager::BlockingBehavior block,
                       scoped_ptr<AbstractLock>* creation_lock);

 private:
  GoogleString file_prefix_;
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlAsyncFetcher* url_async_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;

  // Counts how many URLs we reject because they come from a domain that
  // is not authorized.
  Variable* resource_url_domain_rejections_;

  // Counts how many times we had a cache-hit for the output resource
  // partitioning, but it came too late to be used for the rewrite.
  Variable* cached_output_missed_deadline_;

  // Counts how many times we had a successful cache-hit for output
  // resource partitioning.
  Variable* cached_output_hits_;

  // Counts how many times we had a cache-miss for output
  // resource partitioning.
  Variable* cached_output_misses_;

  // Tracks 404s sent to clients for resource requests.
  Variable* resource_404_count_;

  // Tracks 404s sent clients to when slurping.
  Variable* slurp_404_count_;

  HTTPCache* http_cache_;
  CacheInterface* metadata_cache_;
  bool relative_path_;
  bool store_outputs_in_file_system_;
  NamedLockManager* lock_manager_;
  GoogleString max_age_string_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

class ResourceManagerHttpCallback : public HTTPCache::Callback {
 public:
  ResourceManagerHttpCallback(Resource::AsyncCallback* resource_callback,
                              ResourceManager* resource_manager)
      : resource_callback_(resource_callback),
        resource_manager_(resource_manager) {
  }
  virtual ~ResourceManagerHttpCallback();
  virtual void Done(HTTPCache::FindResult find_result);

 private:
  Resource::AsyncCallback* resource_callback_;
  ResourceManager* resource_manager_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
