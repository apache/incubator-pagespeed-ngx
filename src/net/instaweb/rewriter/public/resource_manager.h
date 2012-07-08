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

#include <cstddef>                     // for size_t
#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class BlinkCriticalLineDataFinder;
class CacheInterface;
class ContentType;
class CriticalImagesFinder;
class FileSystem;
class FilenameEncoder;
class Function;
class FuriousMatcher;
class GoogleUrl;
class Hasher;
class MessageHandler;
class NamedLock;
class NamedLockManager;
class PropertyCache;
class ResponseHeaders;
class RewriteDriver;
class RewriteDriverFactory;
class RewriteOptions;
class RewriteStats;
class Scheduler;
class StaticJavascriptManager;
class Statistics;
class ThreadSynchronizer;
class ThreadSystem;
class Timer;
class UrlAsyncFetcher;
class UrlNamer;
class UsageDataReporter;
class UserAgentMatcher;

typedef RefCountedPtr<OutputResource> OutputResourcePtr;
typedef std::vector<OutputResourcePtr> OutputResourceVector;

// TODO(jmarantz): Rename this class to ServerContext, as it no longer
// contains much logic about resources -- that's been moved to RewriteDriver,
// which should be renamed RequestContext.
class ResourceManager {
 public:
  // The lifetime for cache-extended generated resources, in milliseconds.
  static const int64 kGeneratedMaxAgeMs;

  // This value is a shared constant so that it can also be used in
  // the Apache-specific code that repairs our caching headers downstream
  // of mod_headers.
  static const char kResourceEtagValue[];
  static const char kCacheKeyResourceNamePrefix[];

  // Default statistics group name.
  static const char kStatisticsGroup[];

  explicit ResourceManager(RewriteDriverFactory* factory);
  virtual ~ResourceManager();

  // Set time and cache headers with long TTL (including Date, Last-Modified,
  // Cache-Control, Etags, Expires).
  //
  // Also sets Content-Type headers if content_type is provided.
  // If content_type is null, the Content-Type header is omitted.
  void SetDefaultLongCacheHeaders(const ContentType* content_type,
                                  ResponseHeaders* header) const {
    SetDefaultLongCacheHeadersWithCharset(content_type, StringPiece(), header);
  }

  // As above, but also sets charset if it's non-empty and content_type
  // is non-NULL.
  void SetDefaultLongCacheHeadersWithCharset(
      const ContentType* content_type, StringPiece charset,
      ResponseHeaders* header) const;

  // Changes the content type of a pre-initialized header.
  void SetContentType(const ContentType* content_type, ResponseHeaders* header);

  void set_filename_prefix(const StringPiece& file_prefix);
  void set_statistics(Statistics* x) { statistics_ = x; }
  void set_rewrite_stats(RewriteStats* x) { rewrite_stats_ = x; }
  void set_relative_path(bool x) { relative_path_ = x; }
  void set_lock_manager(NamedLockManager* x) { lock_manager_ = x; }
  void set_http_cache(HTTPCache* x) { http_cache_ = x; }
  void set_page_property_cache(PropertyCache* x) { page_property_cache_ = x; }
  void set_client_property_cache(PropertyCache* x) {
    client_property_cache_ = x;
  }
  void set_metadata_cache(CacheInterface* x) { metadata_cache_ = x; }
  void set_message_handler(MessageHandler* x) { message_handler_ = x; }

  StringPiece filename_prefix() const { return file_prefix_; }
  Statistics* statistics() const { return statistics_; }
  NamedLockManager* lock_manager() const { return lock_manager_; }
  RewriteDriverFactory* factory() const { return factory_; }
  ThreadSynchronizer* thread_synchronizer() {
    return thread_synchronizer_.get();
  }
  FuriousMatcher* furious_matcher() { return furious_matcher_.get(); }

  // Writes the specified contents into the output resource, and marks it
  // as optimized. 'inputs' described the input resources that were used
  // to construct the output, and is used to determine whether the
  // result can be safely cache extended and be marked publicly cacheable.
  // 'content_type' and 'charset' specify the mimetype and encoding of
  // the contents, and will help form the Content-Type header.
  // 'charset' may be empty when not specified.
  //
  // Note that this does not escape charset.
  //
  // Callers should take care that dangerous types like 'text/html' do not
  // sneak into content_type.
  bool Write(const ResourceVector& inputs, const StringPiece& contents,
             const ContentType* content_type, StringPiece charset,
             OutputResource* output, MessageHandler* handler);

  // Computes the most restrictive Cache-Control intersection of the input
  // resources, and the provided headers, and sets that cache-control on the
  // provided headers.  Does nothing if all of the resources are fully
  // cacheable, since in that case we will want to cache-extend.
  //
  // Disregards Cache-Control directives other than max-age, no-cache, no-store,
  // and private, and strips them if any resource is no-cache or private.  By
  // assumption, a resource can only be no-store if it is also no-cache.
  void ApplyInputCacheControl(const ResourceVector& inputs,
                              ResponseHeaders* headers);

  // Is this URL a ref to a Pagespeed resource?
  bool IsPagespeedResource(const GoogleUrl& url);

  // Returns true if the resource with given date and TTL is going to expire
  // shortly and should hence be proactively re-fetched.
  bool IsImminentlyExpiring(int64 start_date_ms, int64 expire_ms) const;

  void ComputeSignature(RewriteOptions* rewrite_options);

  // TODO(jmarantz): check thread safety in Apache.
  Hasher* hasher() const { return hasher_; }
  const Hasher* lock_hasher() const { return &lock_hasher_; }
  const Hasher* contents_hasher() const { return &contents_hasher_; }
  FileSystem* file_system() { return file_system_; }
  void set_file_system(FileSystem* fs ) { file_system_ = fs; }
  FilenameEncoder* filename_encoder() const { return filename_encoder_; }
  void set_filename_encoder(FilenameEncoder* x) { filename_encoder_ = x; }
  UrlNamer* url_namer() const { return url_namer_; }
  void set_url_namer(UrlNamer* n) { url_namer_ = n; }
  StaticJavascriptManager* static_javascript_manager() const {
    return static_javascript_manager_;
  }
  void set_static_javascript_manager(StaticJavascriptManager* manager) {
    static_javascript_manager_ = manager;
  }
  Scheduler* scheduler() const { return scheduler_; }
  void set_scheduler(Scheduler* s) { scheduler_ = s; }
  bool has_default_system_fetcher() { return default_system_fetcher_ != NULL; }

  // Note: for rewriting user content, you want to use RewriteDriver's
  // async_fetcher() instead, as it may apply session-specific optimizations.
  UrlAsyncFetcher* DefaultSystemFetcher() { return default_system_fetcher_; }

  Timer* timer() const { return http_cache_->timer(); }
  HTTPCache* http_cache() const { return http_cache_; }
  PropertyCache* page_property_cache() const { return page_property_cache_; }
  PropertyCache* client_property_cache() const {
    return client_property_cache_;
  }
  CriticalImagesFinder* critical_images_finder() const {
    return critical_images_finder_;
  }
  void set_critical_images_finder(CriticalImagesFinder* finder) {
    critical_images_finder_ = finder;
  }
  const UserAgentMatcher& user_agent_matcher() const {
    return *user_agent_matcher_;
  }
  void set_user_agent_matcher(UserAgentMatcher* n) { user_agent_matcher_ = n; }

  BlinkCriticalLineDataFinder* blink_critical_line_data_finder() const {
    return blink_critical_line_data_finder_;
  }

  void set_blink_critical_line_data_finder(
      BlinkCriticalLineDataFinder* finder) {
    blink_critical_line_data_finder_ = finder;
  }

  // Cache for small non-HTTP objects.
  //
  // Note that this might share namespace with the HTTP cache, so make sure
  // your key names do not start with http://.
  CacheInterface* metadata_cache() { return metadata_cache_; }

  // Whether or not dumps of rewritten resources should be stored to
  // the filesystem. This is meant for testing purposes only.
  bool store_outputs_in_file_system() { return store_outputs_in_file_system_; }
  void set_store_outputs_in_file_system(bool store) {
    store_outputs_in_file_system_ = store;
  }

  void RefreshIfImminentlyExpiring(Resource* resource,
                                   MessageHandler* handler) const;

  RewriteStats* rewrite_stats() const { return rewrite_stats_; }
  MessageHandler* message_handler() const { return message_handler_; }

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents are cached, the callback will
  // be called directly, rather than asynchronously.  The resource
  // will be passed to the callback, with its contents and headers filled in.
  void ReadAsync(Resource::NotCacheablePolicy not_cacheable_policy,
                 Resource::AsyncCallback* callback);

  // Allocate an NamedLock to guard the creation of the given resource.  If the
  // object is expensive to create, this lock should be held during its creation
  // to avoid multiple rewrites happening at once.  The lock will be unlocked
  // when creation_lock is reset or destructed.
  NamedLock* MakeCreationLock(const GoogleString& name);

  // Attempt to obtain a named lock without blocking.  Return true if we do so.
  bool TryLockForCreation(NamedLock* creation_lock);

  // Attempt to obtain a named lock. When the lock has been obtained, queue the
  // callback on the  given worker Sequence.  If the lock times out, cancel the
  // callback, running the cancel on the worker.
  void LockForCreation(NamedLock* creation_lock,
                       QueuedWorkerPool::Sequence* worker, Function* callback);

  // Setters should probably only be used in testing.
  void set_hasher(Hasher* hasher) { hasher_ = hasher; }
  void set_default_system_fetcher(UrlAsyncFetcher* fetcher) {
    default_system_fetcher_ = fetcher;
  }

  // Handles an incoming beacon request by incrementing the appropriate
  // variables.  Returns true if the url was parsed and handled correctly; in
  // this case a 204 No Content response should be sent.  Returns false if the
  // url could not be parsed; in this case the request should be declined.
  bool HandleBeacon(const StringPiece& unparsed_url);

  // Returns a RewriteDriver* suitable for decoding pagespeed URLs.  This
  // driver does not necessarily have up-to-date Options, especially in the
  // case of a cache flush.  However, this doesn't affect its ability to
  // correctly decode options.
  RewriteDriver* decoding_driver() const { return decoding_driver_.get(); }

  // Returns a pointer to the master global_options.  These are not used
  // directly in RewriteDrivers, but are Cloned into the drivers as they
  // are created.  We generally do not expect global_options() to change once
  // the system is processing requests, except in Apache when someone does
  // a cache-flush by touching a file "cache.flush" in the file-cache directory.
  RewriteOptions* global_options();

  // Makes a new, empty set of RewriteOptions.
  RewriteOptions* NewOptions();

  // Generates a new managed RewriteDriver using the RewriteOptions
  // managed by this class.  Each RewriteDriver is not thread-safe,
  // but you can generate a RewriteDriver* for each thread.  The
  // returned drivers manage themselves: when the HTML parsing and
  // rewriting is done they will be returned to the pool.
  //
  // Filters allocated using this mechanism have their filter-chain
  // already frozen (see AddFilters()).
  RewriteDriver* NewRewriteDriver();

  // Generates a new unmanaged RewriteDriver using the RewriteOptions
  // managed by this class.  Each RewriteDriver is not thread-safe,
  // but you can generate a RewriteDriver* for each thread.  The
  // returned drivers must be explicitly deleted by the caller.
  //
  // RewriteDrivers allocated using this mechanism have not yet frozen
  // their filters, and so callers may explicitly enable individual
  // filters on the driver -- beyond those indicated in the options.
  // After all extra filters are added, AddFilters must be called to
  // freeze them and instantiate the filter-chain.
  //
  // Takes ownership of 'options'.
  RewriteDriver* NewUnmanagedRewriteDriver(
      bool is_custom, RewriteOptions* options);

  // Like NewUnmanagedRewriteDriver, but uses supplied options instead
  // of RewriteDriver's global_options().
  //
  // NOTE: This does not merge custom_options with global_options(), the
  // caller must do that if they want them merged.
  //
  // Filters allocated using this mechanism have their filter-chain
  // already frozen (see AddFilters()).
  //
  // Takes ownership of 'custom_options'.
  RewriteDriver* NewCustomRewriteDriver(RewriteOptions* custom_options);

  // Puts a RewriteDriver back on the free pool.  This is intended to
  // be called by a RewriteDriver on itself, once all pending
  // activites on it have completed, including HTML Parsing
  // (FinishParse) and all pending Rewrites.
  //
  // TODO(jmarantz): this cannot recycle RewriteDrivers with custom
  // rewrite options, which is a potential performance issue for Apache
  // installations that set custom options in .htaccess files, where
  // essentially every RewriteDriver will be a custom driver.  To
  // resolve this we need to make a comparator for RewriteOptions
  // so that we can determine option-equivalence and, potentially,
  // keep free-lists for each unique option-set.
  void ReleaseRewriteDriver(RewriteDriver* rewrite_driver);

  ThreadSystem* thread_system() { return thread_system_; }
  UsageDataReporter* usage_data_reporter() { return usage_data_reporter_; }

  // Calling this method will stop results of rewrites being cached in the
  // metadata cache. This is meant for the shutdown sequence.
  void set_metadata_cache_readonly() {
    metadata_cache_readonly_.set_value(true);
  }

  bool metadata_cache_readonly() const {
    return metadata_cache_readonly_.value();
  }

  // Waits a bounded amount of time for all currently running jobs to
  // complete.  This is meant for use when shutting down processing,
  // so that jobs running in background do not access objects that are
  // about to be deleted.  If there are long-running outstanding tasks,
  // the drivers may stay running past this call.
  //
  // TODO(jmarantz): Change New*RewriteDriver() calls to return NULL
  // when run after shutdown.  This requires changing call-sites to
  // null-check their drivers and gracefully fail.
  void ShutDownDrivers();

  // Take any headers that are not caching-related, and not otherwise
  // filled in by SetDefaultLongCacheHeaders or SetContentType, but
  // *were* set on the input resource, and copy them to the output
  // resource.  This allows user headers to be preserved.  This must
  // be called as needed by individual filters, prior to Write().
  //
  // Note that this API is only usable for single-input rewriters.
  // Combiners will need to execute some kind of merge, union, or
  // intersection policy, if we wish to preserve origin response
  // headers.
  //
  // Note: this does not call ComputeCaching() on the output headers,
  // so that method must be called prior to invoking any caching predicates
  // on the output's ResponseHeader.  In theory we shouldn't mark the
  // caching bits dirty because we are only adding headers that will
  // not affect caching, but at the moment the dirty-bit is set independent
  // of that.
  //
  // TODO(jmarantz): avoid setting caching_dirty bit in ResponseHeaders when
  // the header is not caching-related.
  void MergeNonCachingResponseHeaders(const ResourcePtr& input,
                                      const OutputResourcePtr& output) {
    MergeNonCachingResponseHeaders(*input->response_headers(),
                                   output->response_headers());
  }

  // Entry-point with the same functionality, exposed for easier testing.
  void MergeNonCachingResponseHeaders(const ResponseHeaders& input_headers,
                                      ResponseHeaders* output_headers);

  // Pool of worker-threads that can be used to handle html-parsing.
  QueuedWorkerPool* html_workers() { return html_workers_; }

  // Pool of worker-threads that can be used to handle resource rewriting.
  QueuedWorkerPool* rewrite_workers() { return rewrite_workers_; }

  // Pool of worker-threads that can be used to handle low-priority/high CPU
  // portions of resource rewriting.
  QueuedWorkerPool* low_priority_rewrite_workers() {
    return low_priority_rewrite_workers_;
  }

  // Returns the number of rewrite drivers that we were aware of at the
  // time of the call. This includes those created via NewCustomRewriteDriver
  // and NewRewriteDriver, but not via NewUnmanagedRewriteDriver.
  size_t num_active_rewrite_drivers();

  // A ResourceManager may be created in one phase, and later populated
  // with all its dependencies.  This populates the worker threads and
  // a RewriteDriver used just for quickly decoding (but not serving) URLs.
  void InitWorkersAndDecodingDriver();

  // Returns whether or not this attribute can be merged into headers
  // without additional considerations.
  static bool IsExcludedAttribute(const char* attribute);

  // Determines whether we can assume that the response headers we see
  // in rewrite_drivers when filters are applied reflect the final
  // form from the origin.  In proxy applications, this is generally
  // true.  But in Apache, it depends when the output_filter is
  // applied relative to mod_headers and mod_expires.
  //
  // The default-value is 'true'.
  bool response_headers_finalized() const {
    return response_headers_finalized_;
  }
  void set_response_headers_finalized(bool x) {
    response_headers_finalized_ = x;
  }

 private:
  friend class ResourceManagerTest;
  typedef std::set<RewriteDriver*> RewriteDriverSet;

  // Must be called with rewrite_drivers_mutex_ held.
  void ReleaseRewriteDriverImpl(RewriteDriver* rewrite_driver);

  // These are normally owned by the RewriteDriverFactory that made 'this'.
  ThreadSystem* thread_system_;
  RewriteStats* rewrite_stats_;
  GoogleString file_prefix_;
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlNamer* url_namer_;
  UserAgentMatcher* user_agent_matcher_;
  Scheduler* scheduler_;
  UrlAsyncFetcher* default_system_fetcher_;
  Hasher* hasher_;
  CriticalImagesFinder* critical_images_finder_;
  BlinkCriticalLineDataFinder* blink_critical_line_data_finder_;

  // hasher_ is often set to a mock within unit tests, but some parts of the
  // system will not work sensibly if the "hash algorithm" used always returns
  // constants. For those, we have two separate hashers.
  MD5Hasher lock_hasher_;  // Used to compute named lock names.

  // Used to hash file contents to see if inputs to a rewrites have actually
  // changed (and didn't just expire).
  MD5Hasher contents_hasher_;

  Statistics* statistics_;

  HTTPCache* http_cache_;
  PropertyCache* page_property_cache_;
  PropertyCache* client_property_cache_;
  CacheInterface* metadata_cache_;

  bool relative_path_;
  bool store_outputs_in_file_system_;
  bool response_headers_finalized_;

  NamedLockManager* lock_manager_;
  MessageHandler* message_handler_;

  // RewriteDrivers that were previously allocated, but have
  // been released with ReleaseRewriteDriver, and are ready
  // for re-use with NewRewriteDriver.
  // Protected by rewrite_drivers_mutex_.
  std::vector<RewriteDriver*> available_rewrite_drivers_;

  // RewriteDrivers that are currently in use.  This is retained
  // as a sanity check to make sure our system is coherent,
  // and to facilitate complete cleanup if a Shutdown occurs
  // while a request is in flight.
  // Protected by rewrite_drivers_mutex_.
  RewriteDriverSet active_rewrite_drivers_;

  // If this value is true ReleaseRewriteDriver will just insert its
  // argument into deferred_release_rewrite_drivers_ rather
  // than try to delete or recycle it. This is used for shutdown
  // so that the main thread does not have to worry about rewrite threads
  // deleting RewriteDrivers or altering active_rewrite_drivers_.
  //
  // Protected by rewrite_drivers_mutex_.
  bool trying_to_cleanup_rewrite_drivers_;
  RewriteDriverSet deferred_release_rewrite_drivers_;

  // If set, a RewriteDriverFactory provides a mechanism to add
  // platform-specific filters to a RewriteDriver.
  RewriteDriverFactory* factory_;

  scoped_ptr<AbstractMutex> rewrite_drivers_mutex_;

  // Note: this must be before decoding_driver_ since it's needed to init it.
  // All access, even internal to the class, should be via options() so
  // subclasses can override.
  scoped_ptr<RewriteOptions> base_class_options_;

  // Keep around a RewriteDriver just for decoding resource URLs, using
  // the default options.  This is possible because the id->RewriteFilter
  // table is fully constructed independent of the options.
  //
  // TODO(jmarantz): If domain-sharding or domain-rewriting is
  // specified in a Directory scope or .htaccess file, the decoding
  // driver will not see them.  This is blocks effective
  // implementation of these features in environments where all
  // configuration must be done by .htaccess.
  scoped_ptr<RewriteDriver> decoding_driver_;

  QueuedWorkerPool* html_workers_;  // Owned by the factory
  QueuedWorkerPool* rewrite_workers_;  // Owned by the factory
  QueuedWorkerPool* low_priority_rewrite_workers_;  // Owned by the factory

  AtomicBool metadata_cache_readonly_;

  // Used to create URLs for various filter javascript files.
  StaticJavascriptManager* static_javascript_manager_;

  // Used to help inject sync-points into thread-intensive code for the purposes
  // of controlling thread interleaving to test code for possible races.
  scoped_ptr<ThreadSynchronizer> thread_synchronizer_;

  // Used to match clients or sessions to a specific furious experiment.
  scoped_ptr<FuriousMatcher> furious_matcher_;

  UsageDataReporter* usage_data_reporter_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
