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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_

#include <map>
#include <set>
#include <vector>
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/scan_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/printf_format.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

struct ContentType;

class AbstractMutex;
class AddInstrumentationFilter;
class CommonFilter;
class DomainRewriteFilter;
class FileSystem;
class Function;
class HtmlFilter;
class HtmlWriterFilter;
class MessageHandler;
class RequestHeaders;
class ResourceContext;
class ResourceNamer;
class ResponseHeaders;
class RewriteContext;
class RewriteFilter;
class Statistics;
class UrlLeftTrimFilter;
class Writer;

// TODO(jmarantz): rename this class to RequestContext.  This extends
// class HtmlParse (which should renamed HtmlContext) by providing
// context for rewriting resources (css, js, images).
//
// Also note that ResourceManager should be renamed ServerContext, as
// it no longer contains much logic about resources.
class RewriteDriver : public HtmlParse {
 public:
  static const char kCssCombinerId[];
  static const char kCssFilterId[];
  static const char kCacheExtenderId[];
  static const char kImageCombineId[];
  static const char kImageCompressionId[];
  static const char kJavascriptCombinerId[];
  static const char kJavascriptMinId[];

  // A list of HTTP request headers.  These are the headers which
  // should be passed through from the client request into the
  // ResponseHeaders request_headers sent to the rewrite driver.
  // Headers not in this list will be ignored so there is no need to
  // copy them over.
  static const char* kPassThroughRequestAttributes[3];

  RewriteDriver(MessageHandler* message_handler,
                FileSystem* file_system,
                UrlAsyncFetcher* url_async_fetcher);

  // Need explicit destructors to allow destruction of scoped_ptr-controlled
  // instances without propagating the include files.
  virtual ~RewriteDriver();

  // Returns a fresh instance using the same options we do.
  RewriteDriver* Clone();

  // Clears the current request cache of resources and base URL.  The
  // filter-chain is left intact so that a new request can be issued.
  // Deletes all RewriteContexts.
  //
  // WaitForCompletion must be called prior to Clear().
  void Clear();

  // Calls Initialize on all known rewrite_drivers.
  static void Initialize(Statistics* statistics);

  // Adds a resource manager enabling the rewriting of
  // resources. This will replace any previous resource managers.
  void SetResourceManager(ResourceManager* resource_manager);

  // NULL is returned for resources that:
  //  - were not requested during Scan
  //  - were requested, but have not yet finished being retrieved
  //  - were requested, but failed
  //
  // TODO(jmarantz): note that the returned resource does not necessarily
  // have its content loaded. This needs some more design work.
  bool FindResource(const StringPiece& url, ResourcePtr* resource) const;

  // Determines whether images should be rewritten.
  // TODO(jmarantz): Another approach to allow rewriting to be friendly
  // to image-search link rel=canonical, described in:
  //   http://googlewebmastercentral.blogspot.com/2011/06/
  //   supporting-relcanonical-http-headers.html
  bool ShouldNotRewriteImages() const;

  void RememberResource(const StringPiece& url, const ResourcePtr& resource);
  const GoogleString& user_agent() const {
    return user_agent_;
  }
  inline void set_user_agent(const StringPiece& user_agent_string) {
    user_agent_string.CopyToString(&user_agent_);
  }

  // Return a pointer to the response headers that filters can update
  // before the frist flush.
  ResponseHeaders* response_headers_ptr() {
    return response_headers_;
  }

  // Set the pointer to the response headers that filters can update
  // before the first flush.  RewriteDriver does NOT take ownership
  // of this memory.
  void set_response_headers_ptr(ResponseHeaders* headers) {
    response_headers_ = headers;
  }

  const UserAgentMatcher& user_agent_matcher() const {
    return user_agent_matcher_;
  }
  bool UserAgentSupportsImageInlining() const {
    return user_agent_matcher_.SupportsImageInlining(user_agent_);
  }
  bool UserAgentSupportsWebp() const {
    return user_agent_matcher_.SupportsWebp(user_agent_);
  }

  // Adds the filters from the options, specified by name in enabled_filters.
  // This must be called explicitly after object construction to provide an
  // opportunity to programatically add custom filters beyond those defined
  // in RewriteOptions, via AddFilter(HtmlFilter* filter) (below).
  void AddFilters();

  // Adds a filter to the pre-render chain, taking ownership.
  void AddOwnedPreRenderFilter(HtmlFilter* filter);

  // Adds a filter to the post-render chain, taking ownership.
  void AddOwnedPostRenderFilter(HtmlFilter* filter);
  // Same, without taking ownership.
  void AddUnownedPostRenderFilter(HtmlFilter* filter);

  // Add a RewriteFilter to the pre-render chain and take ownership of
  // the filter.  This differs from AddOwnedPreRenderFilter in that
  // it adds the filter's ID into a dispatch table for serving
  // rewritten resources.  E.g. if your filter->id == "xy" and
  // FetchResource("NAME.pagespeed.xy.HASH.EXT"...)  is called, then
  // RewriteDriver will dispatch to filter->Fetch().
  //
  // This is used when the filter being added is not part of the
  // core set built into RewriteDriver and RewriteOptions, such
  // as platform-specific or server-specific filters, or filters
  // invented for unit-testing the framework.
  void AddRewriteFilter(RewriteFilter* filter);

  // Controls how HTML output is written.  Be sure to call this last, after
  // all other filters have been established.
  //
  // TODO(jmarantz): fix this in the implementation so that the caller can
  // install filters in any order and the writer will always be last.
  void SetWriter(Writer* writer);

  // Initiates an async fetch for a rewritten resource with the specified name.
  // If resource matches the pattern of what the driver is authorized to serve,
  // then true is returned and the caller must listen on the callback for the
  // completion of the request.
  //
  // If the pattern does not match, then false is returned, and the request
  // should be passed to another handler, and the callback will *not* be
  // called.  In other words there are four outcomes for this routine:
  //
  //   1. the request was handled immediately and the callback called
  //      before the method returns.  true is returned.
  //   2. the request looks good but was queued because some other resource
  //      fetch is needed to satisfy it.  true is returned.
  //   3. the request looks like one it belongs to Instaweb, but the resource
  //      could not be decoded.  The callback is called immediately with
  //      'false', but true is returned.
  //   4. the request does not look like it belongs to Instaweb.  The callback
  //      will not be called, and false will be returned.
  //
  // In other words, if this routine returns 'false' then the callback
  // will not be called.  If the callback is called, then this should be the
  // 'final word' on this request, whether it was called with success=true or
  // success=false.
  bool FetchResource(const StringPiece& resource,
                     const RequestHeaders& request_headers,
                     ResponseHeaders* response_headers,
                     Writer* writer,
                     UrlAsyncFetcher::Callback* callback);

  // See FetchResource.  There are two differences:
  //   1. It takes an OutputResource instead of a URL.
  //   2. It returns whether a fetch was queued or not.  This is safe
  //      to ignore because in either case the callback will be called.
  //   3. If 'filter' is NULL then the request only checks cache and
  //      (if enabled) the file system.
  bool FetchOutputResource(const OutputResourcePtr& output_resource,
                           RewriteFilter* filter,
                           const RequestHeaders& request_headers,
                           ResponseHeaders* response_headers,
                           Writer* writer,
                           UrlAsyncFetcher::Callback* callback);

  // Attempts to decode an output resource based on the URL pattern
  // without actually rewriting it. No permission checks are performed on the
  // url, though it is parsed to see if it looks like the url of a generated
  // resource (which should mean checking the hash to ensure we generated it
  // ourselves).
  // TODO(jmaessen): add url hash & check thereof.
  OutputResourcePtr DecodeOutputResource(const GoogleUrl& url,
                                         RewriteFilter** filter);

  // As above, but does not actually create a resource object,
  // and instead outputs the decoded information into the various out
  // parameters. Returns whether decoding successful or not.
  bool DecodeOutputResourceName(const GoogleUrl& url,
                                ResourceNamer* name_out,
                                OutputResourceKind* kind_out,
                                RewriteFilter** filter_out);

  FileSystem* file_system() { return file_system_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

  // Takes ownership of 'options'.
  void set_custom_options(RewriteOptions* options) {
    custom_options_.reset(options);
  }

  // Determines whether this RewriteDriver was built with custom options.
  bool has_custom_options() const { return (custom_options_.get() != NULL); }

  // Return the options used for this RewriteDriver.
  const RewriteOptions* options() const {
    return (has_custom_options() ? custom_options_.get()
            : resource_manager_->global_options());
  }

  // Override HtmlParse's StartParseId to propagate any required options.
  virtual bool StartParseId(const StringPiece& url, const StringPiece& id,
                            const ContentType& content_type);

  // Override HtmlParse's FinishParse to ensure that the
  // request-scoped cache is cleared immediately.
  //
  // Note that the RewriteDriver can delete itself in this method, if
  // it's not externally managed, and if all RewriteContexts have been
  // completed.
  virtual void FinishParse();

  // As above, but asynchronous. Note that the RewriteDriver may already be
  // deleted at the point the callback is invoked.
  void FinishParseAsync(Function* callback);

  // Report error message with description of context's location
  // (such as filenames and line numbers). context may be NULL, in which case
  // the current parse position will be used.
  void InfoAt(RewriteContext* context,
              const char* msg, ...) INSTAWEB_PRINTF_FORMAT(3, 4);

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
      const StringPiece& filter_prefix,
      const UrlSegmentEncoder* encoder,
      const ResourceContext* data,
      const ResourcePtr& input_resource,
      OutputResourceKind kind,
      bool use_async_flow);

  // Creates an output resource where the name is provided.  The intent is to
  // be able to derive the content from the name, for example, by encoding
  // URLs and metadata.
  //
  // This method succeeds unless the filename is too long.
  //
  // This name is prepended with path for writing hrefs, and the resulting url
  // is encoded and stored at file_prefix when working with the file system.
  // So hrefs are:
  //    $(PATH)/$(NAME).pagespeed.$(FILTER_PREFIX).$(HASH).$(CONTENT_TYPE_EXT)
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  //
  // Could be private since you should use one of the versions below but put
  // here with the rest like it and for documentation clarity.
  OutputResourcePtr CreateOutputResourceWithPath(
      const StringPiece& mapped_path, const StringPiece& unmapped_path,
      const StringPiece& base_url, const StringPiece& filter_id,
      const StringPiece& name, const ContentType* content_type,
      OutputResourceKind kind, bool use_async_flow);

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths are the same and the base_url is this driver's base_url.
  OutputResourcePtr CreateOutputResourceWithUnmappedPath(
      const StringPiece& path, const StringPiece& filter_id,
      const StringPiece& name, const ContentType* content_type,
      OutputResourceKind kind, bool use_async_flow) {
    return CreateOutputResourceWithPath(path, path, base_url_.AllExceptLeaf(),
                                        filter_id, name, content_type, kind,
                                        use_async_flow);
  }

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths are different and the base_url is this driver's base_url.
  OutputResourcePtr CreateOutputResourceWithMappedPath(
      const StringPiece& mapped_path, const StringPiece& unmapped_path,
      const StringPiece& filter_id, const StringPiece& name,
      const ContentType* content_type, OutputResourceKind kind,
      bool use_async_flow) {
    return CreateOutputResourceWithPath(mapped_path, unmapped_path,
                                        base_url_.AllExceptLeaf(),
                                        filter_id, name, content_type, kind,
                                        use_async_flow);
  }

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths and the base url are all the same. FOR TESTS ONLY.
  OutputResourcePtr CreateOutputResourceWithPath(
      const StringPiece& path, const StringPiece& filter_id,
      const StringPiece& name, const ContentType* content_type,
      OutputResourceKind kind, bool use_async_flow) {
    return CreateOutputResourceWithPath(path, path, path, filter_id, name,
                                        content_type, kind, use_async_flow);
  }

  // Creates an input resource based on input_url.  Returns NULL if
  // the input resource url isn't valid, or can't legally be rewritten in the
  // context of this page.
  ResourcePtr CreateInputResource(const GoogleUrl& input_url);

  // Creates an input resource from the given absolute url.  Requires that the
  // provided url has been checked, and can legally be rewritten in the current
  // page context.
  ResourcePtr CreateInputResourceAbsoluteUnchecked(
      const StringPiece& absolute_url);

  // Checks to see if we can write the input_url resource in the
  // domain_url taking into account domain authorization and
  // wildcard allow/disallow from RewriteOptions.
  bool MayRewriteUrl(const GoogleUrl& domain_url,
                     const GoogleUrl& input_url) const;

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents are cached, the callback will
  // be called directly, rather than asynchronously.  The resource
  // will be passed to the callback, with its contents and headers filled in.
  void ReadAsync(Resource::AsyncCallback* callback,
                 MessageHandler* message_handler);

  // Load the resource if it is cached (or if it can be fetched quickly).
  // If not send off an asynchronous fetch and store the result in the cache.
  //
  // Returns true if the resource is loaded.
  bool ReadIfCached(const ResourcePtr& resource);

  // As above, but distinguishes between unavailable in cache and not found
  HTTPCache::FindResult ReadIfCachedWithStatus(const ResourcePtr& resource);

  // Returns the appropriate base gurl to be used for resolving hrefs
  // in the document.  Note that HtmlParse::google_url() is the URL
  // for the HTML file and is used for printing html syntax errors.
  const GoogleUrl& base_url() const { return base_url_; }

  // Returns the decoded version of base_gurl() in case it was encoded by a
  // non-default UrlNamer (for the default UrlNamer this returns the same value
  // as base_url()).  Required when fetching a resource by its encoded name.
  StringPiece decoded_base() const { return decoded_base_url_.Spec(); }

  const UrlSegmentEncoder* default_encoder() const { return &default_encoder_; }

  // Finds a filter with the given ID, or returns NULL if none found.
  RewriteFilter* FindFilter(const StringPiece& id) const;

  // Returns refs_before_base.
  bool refs_before_base() { return refs_before_base_; }

  // Sets whether or not there were references to urls before the
  // base tag (if there is a base tag).  This variable has document-level
  // scope.  It is reset at the beginning of every document by
  // ScanFilter.
  void set_refs_before_base() { refs_before_base_ = true; }

  // Establishes a HtmlElement slot for rewriting.
  HtmlResourceSlotPtr GetSlot(const ResourcePtr& resource,
                              HtmlElement* elt,
                              HtmlElement::Attribute* attr);

  // Method to start a resource rewrite.  This is called by a filter during
  // parsing, although the Rewrite might continue after deadlines expire
  // and the rewritten HTML must be flushed.
  void InitiateRewrite(RewriteContext* rewrite_context);
  void InitiateFetch(RewriteContext* rewrite_context);

  // Provides a mechanism for a RewriteContext to notify a
  // RewriteDriver that it is complete, to allow the RewriteDriver
  // to delete itself or return it back to a free pool in the ResourceManager.
  void RewriteComplete(RewriteContext* rewrite_context);

  // Provides a mechanism for a RewriteContext to notify a
  // RewriteDriver that a certain number of rewrites have been discovered
  // to need to take the slow path.
  void ReportSlowRewrites(int num);

  // If there are not outstanding references to this RewriteDriver,
  // delete it or recycle it to a free pool in the ResourceManager.
  // If this is a fetch, calling this also signals to the system that you
  // are no longer interested in its results.
  void Cleanup();

  // Wait for outstanding Rewrite to complete.  Once the rewrites are
  // complete they can be rendered or deleted.
  void WaitForCompletion();

  // As above, but with a time bound. Non-positive values of timeout disable it.
  void BoundedWaitForCompletion(int64 timeout_ms);

  // Renders any completed rewrites back into the DOM.
  void Render();

  // Experimental asynchronous rewrite feature.  This is present only
  // for regression tests, and should not be used in production.
  bool asynchronous_rewrites() const { return asynchronous_rewrites_; }
  void SetAsynchronousRewrites(bool x);

  // Indicate that this RewriteDriver will be explicitly deleted, and
  // thus should not be auto-deleted at the end of the parse.  This is
  // primarily for tests.
  //
  // TODO(jmarantz): Consider phasing this out to make tests behave
  // more like servers.
  void set_externally_managed(bool x) { externally_managed_ = x; }

  // Called by RewriteContext when an async fetch is complete, allowing
  // the RewriteDriver to be recycled.
  void FetchComplete();

  // Deletes the specified RewriteContext.  If this is the last RewriteContext
  // active on this Driver, and there is no other outstanding activity, then
  // the RewriteDriver itself can be recycled, and WaitForCompletion can return.
  //
  // We expect to this method to be called on the Rewrite thread.
  void DeleteRewriteContext(RewriteContext* rewrite_context);

  // Explicitly sets the number of milliseconds to wait for Rewrites
  // to complete while HTML parsing, overriding a default value which
  // is dependent on whether the system is compiled for debug or
  // release, or whether it's been detected as running on valgrind
  // at runtime.
  void set_rewrite_deadline_ms(int x) { rewrite_deadline_ms_ = x; }

  // Tries to register the given rewrite context as working on
  // its partition key. If this context is the first one to try to handle it,
  // returns NULL. Otherwise returns the previous such context.
  //
  // Must only be called from rewrite thread.
  RewriteContext* RegisterForPartitionKey(const GoogleString& partition_key,
                                          RewriteContext* candidate);

  // Must be called after all other rewrites that are currently relying on this
  // one have had their RepeatedSuccess or RepeatedFailure methods called.
  //
  // Must only be called from rewrite thread.
  void DeregisterForPartitionKey(
      const GoogleString& partition_key, RewriteContext* candidate);

  // Indicates that a Flush through the HTML parser chain should happen
  // soon, e.g. once the network pauses its incoming byte stream.
  void RequestFlush() { flush_requested_ = true; }

  // Executes an Flush() if RequestFlush() was called, e.g. from the
  // Listener Filter (see set_event_listener below).  Consider an HTML
  // parse driven by a UrlAsyncFetcher.  When the UrlAsyncFetcher
  // temporarily runs out of bytes to read, it calls
  // response_writer->Flush().  When that happens, we may want to
  // consider flushing the outstanding HTML events through the system
  // so that the browser can start fetching subresources and
  // rendering.  The event_listener (see set_event_listener below)
  // helps determine whether enough "interesting" events have passed
  // in the current flush window so that we should take this incoming
  // network pause as an opportunity.
  void ExecuteFlushIfRequested();

  // Asynchronous version of the above. Note that you should not
  // attempt to write out any data until the callback is invoked.
  // (If a flush is not needed, the callback will be invoked immediately).
  void ExecuteFlushIfRequestedAsync(Function* callback);

  // Overrides HtmlParse::Flush so that it can happen in two phases:
  //    1. Pre-render chain runs, resulting in async rewrite activity
  //    2. async rewrite activity ends, calling callback, and post-render
  //       filters run.
  // This API is used for unit-tests & Apache (which lacks a useful event
  // model) and results in blocking behavior.
  //
  // FlushAsync is prefered for event-driven servers.
  virtual void Flush();

  // Initiates an asynchronous Flush.  done->Run() will be called when
  // the flush is complete.  Further calls to ParseText should be
  // deferred until the callback is called.
  void FlushAsync(Function* done);

  // Queues up a task to run on the (high-priority) rewrite thread.
  void AddRewriteTask(Function* task);

  // Queues up a task to run on the low-priority rewrite thread.
  // Such tasks are expected to be safely cancelable.
  void AddLowPriorityRewriteTask(Function* task);

  QueuedWorkerPool::Sequence* html_worker() { return html_worker_; }
  QueuedWorkerPool::Sequence* rewrite_worker() { return rewrite_worker_; }
  QueuedWorkerPool::Sequence* low_priority_rewrite_worker() {
    return low_priority_rewrite_worker_;
  }

  Scheduler* scheduler() { return scheduler_; }

  // Used by CacheExtender, CssCombineFilter, etc. for rewriting domains
  // of sub-resources in CSS.
  DomainRewriteFilter* domain_rewriter() { return domain_rewriter_.get(); }
  UrlLeftTrimFilter* url_trim_filter() { return url_trim_filter_.get(); }

 private:
  friend class ResourceManagerTestBase;
  friend class ResourceManagerTest;

  typedef std::map<GoogleString, RewriteFilter*> StringFilterMap;
  typedef void (RewriteDriver::*SetStringMethod)(const StringPiece& value);
  typedef void (RewriteDriver::*SetInt64Method)(int64 value);

  enum WaitMode {
    kWaitForCompletion,   // wait for everything to complete (upto deadline)
    kWaitForCachedRender  // wait for at least cached rewrites to complete,
                          // and anything else that finishes within deadline.
  };

  // Checks whether outstanding rewrites are completed in a satisfactory
  // fashion with respect to given wait_mode and timeout, and invokes
  // done->Run() when either finished or timed out.
  // Assumes rewrite_mutex held.
  void CheckForCompletionAsync(WaitMode wait_mode, int64 timeout_ms,
                               Function* done);

  // A single check attempt for the above. Will either invoke callback
  // or ask scheduler to check again.
  // Assumes rewrite_mutex held.
  void TryCheckForCompletion(WaitMode wait_mode, int64 end_time_ms,
                             Function* done);

  // Termination predicate for above; assumes locks held.
  bool IsDone(WaitMode wait_mode, bool deadline_reached);

  // Portion of flush that happens asynchronously off the scheduler
  // once the rendering is complete. Calls back to 'callback' after its
  // processing, but with the lock released.
  void FlushAsyncDone(int num_rewrites, Function* callback);

  // Queues up invocation of FlushAsyncDone in our html_workers sequence.
  void QueueFlushAsyncDone(int num_rewrites, Function* callback);

  // Called as part of implementation of FinishParseAsync, after the
  // flush is complete.
  void QueueFinishParseAfterFlush(Function* user_callback);
  void FinishParseAfterFlush(Function* user_callback);

  // Must be called with rewrites_mutex_ held.
  bool RewritesComplete() const;

  // Sets the base GURL in response to a base-tag being parsed.  This
  // should only be called by ScanFilter.
  void SetBaseUrlIfUnset(const StringPiece& new_base);

  // Sets the base URL for a resource fetch.  This should only be called from
  // test code and from FetchResource.
  void SetBaseUrlForFetch(const StringPiece& url);

  // Saves a decoding of the Base URL in decoded_base_url_.  Use this
  // whenever updating base_url_.
  void SetDecodedUrlFromBase();

  // The rewrite_mutex is owned by the scheduler.
  AbstractMutex* rewrite_mutex() { return scheduler_->mutex(); }

  friend class ScanFilter;

  bool ParseKeyString(const StringPiece& key, SetStringMethod m,
                      const GoogleString& flag);
  bool ParseKeyInt64(const StringPiece& key, SetInt64Method m,
                     const GoogleString& flag);

  // Adds a CommonFilter into the HtmlParse filter-list, and into the
  // Scan filter-list for initiating async resource fetches.   See
  // ScanRequestUrl above.
  void AddCommonFilter(CommonFilter* filter);

  // Registers RewriteFilter in the map, but does not put it in the
  // html parse filter chain.  This allows it to serve resource
  // requests.
  void RegisterRewriteFilter(RewriteFilter* filter);

  // Adds an already-owned rewrite filter to the pre-render chain.  This
  // is used for filters that are unconditionally created for handling of
  // resources, but their presence in the html-rewrite chain is conditional
  // on options.
  void EnableRewriteFilter(const char* id);

  // Internal low-level helper for resource creation.
  // Use only when permission checking has been done explicitly on the
  // caller side.
  ResourcePtr CreateInputResourceUnchecked(const GoogleUrl& gurl);

  void AddPreRenderFilters();
  void AddPostRenderFilters();

  // Only the first base-tag is significant for a document -- any subsequent
  // ones are ignored.  There should be no URLs referenced prior to the base
  // tag, if one exists.  See
  //
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //    semantics.html#the-base-element
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //    urls.html#document-base-url
  //
  // Thus we keep the base-tag in the RewriteDriver, and also keep track of
  // whether it's been reset already within the document.
  bool base_was_set_;

  // Stores whether or not there were references to urls before the
  // base tag (if there is a base tag) in this document.  If there is
  // no base tag, this should be false.  If the base tag is before all
  // other url references, this should also be false.
  bool refs_before_base_;

  // Asynchronous rewriting is, at the moment, an experimental-only feature,
  // which can only be turned on for unit tests.
  bool asynchronous_rewrites_;
  bool filters_added_;
  bool externally_managed_;

  // Indicates that a resource fetch has been dispatched to a RewriteContext,
  // and thus the RewriteDriver should not recycled until that RewriteContext
  // has called FetchComplete().
  bool fetch_queued_;            // protected by rewrite_mutex()

  // Indicates that the rewrite driver is currently parsing the HTML,
  // and thus should not be recycled under FinishParse() is called.
  bool parsing_;  // protected by rewrite_mutex()

  // Indicates that WaitForCompletion() has been called in the HTML thread,
  // and we are now blocked on a condition variable in that function.  Thus
  // it only makes sense to examine this from the Rewrite thread.
  bool waiting_for_completion_;  // protected by rewrite_mutex()

  // Likewise for Render() (except when that's emulating WaitForCompletion)
  bool waiting_for_render_;  //  protected by rewrite_mutex()

  // If this is true, this RewriteDriver should Cleanup() itself when it
  // finishes handling the current fetch.
  bool cleanup_on_fetch_complete_;

  bool flush_requested_;

  // Tracks the number of RewriteContexts that have been completed,
  // but not yet deleted.  Once RewriteComplete has been called,
  // rewrite_context->Propagate() is called to render slots (if not
  // detached) and to queue up activity that must occur prior to the
  // context being deleted: specifically running any successors.
  // After all that occurs, DeleteRewriteContext must be called and
  // that will decrement this counter.
  int rewrites_to_delete_;       // protected by rewrite_mutex()

  GoogleUrl base_url_;
  GoogleUrl decoded_base_url_;
  GoogleString user_agent_;
  StringFilterMap resource_filter_map_;

  ResponseHeaders* response_headers_;

  // This group of rewrite-context-related variables is accessed
  // only in the main thread of RewriteDriver (aka the HTML thread).
  typedef std::vector<RewriteContext*> RewriteContextVector;
  RewriteContextVector rewrites_;  // ordered list of rewrites to initiate
  int rewrite_deadline_ms_;

  typedef std::set<RewriteContext*> RewriteContextSet;

  // Contains the RewriteContext* that have been queued into the
  // RewriteThread, but have not gotten to the point where
  // RewriteComplete() has been called.  This set is cleared
  // one the rewrite_deadline_ms has passed.
  RewriteContextSet initiated_rewrites_;  // protected by rewrite_mutex()

  // Contains the RewriteContext* that were still running at the deadline.
  // They are said to be in a "detached" state although the RewriteContexts
  // themselves don't know that.  They will continue performing their
  // Rewrite in the RewriteThread, and caching the results.  And until
  // they complete, the RewriteDriver must stay alive and not be Recycled
  // or deleted.  WaitForCompletion() blocks until all detached_rewrites
  // have been retired.
  RewriteContextSet detached_rewrites_;   // protected by rewrite_mutex()

  // The number of rewrites that have been requested, and not yet
  // completed.  This can actually be derived, more or less, from
  // initiated_rewrites_.size() and rewrites_.size() but is kept
  // separate for programming convenience.
  int pending_rewrites_;                  // protected by rewrite_mutex()

  // Rewrites that may possibly be satisfied from metadata cache alone.
  int possibly_quick_rewrites_;           // protected by rewrite_mutex()

  // These objects are provided on construction or later, and are
  // owned by the caller.
  FileSystem* file_system_;
  UrlAsyncFetcher* url_async_fetcher_;
  ResourceManager* resource_manager_;
  Scheduler* scheduler_;

  AddInstrumentationFilter* add_instrumentation_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
  UserAgentMatcher user_agent_matcher_;

  ScanFilter scan_filter_;
  scoped_ptr<DomainRewriteFilter> domain_rewriter_;
  scoped_ptr<UrlLeftTrimFilter> url_trim_filter_;

  // Maps encoded URLs to output URLs.
  typedef std::map<GoogleString, ResourcePtr> ResourceMap;
  ResourceMap resource_map_;

  // Maps rewrite context partition keys to the context responsible for
  // rewriting them, in case a URL occurs more than once.
  typedef std::map<GoogleString, RewriteContext*> PrimaryRewriteContextMap;
  PrimaryRewriteContextMap primary_rewrite_context_map_;

  HtmlResourceSlotSet slots_;

  scoped_ptr<RewriteOptions> custom_options_;

  // The default resource encoder
  UrlSegmentEncoder default_encoder_;

  // The chain of filters called prior to waiting for Rewrites to complete.
  FilterVector pre_render_filters_;

  // A container of filters to delete when RewriteDriver is deleted.  This
  // can include pre_render_filters as well as those added to the post-render
  // chain owned by HtmlParse.
  FilterVector filters_to_delete_;

  QueuedWorkerPool::Sequence* html_worker_;
  QueuedWorkerPool::Sequence* rewrite_worker_;
  QueuedWorkerPool::Sequence* low_priority_rewrite_worker_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriver);
};

// Subclass of HTTPCache::Callback that incorporates a given RewriteOptions'
// invalidation policy.
class OptionsAwareHTTPCacheCallback : public HTTPCache::Callback {
 public:
  virtual ~OptionsAwareHTTPCacheCallback();
  virtual bool IsCacheValid(const ResponseHeaders& headers);

 protected:
  explicit OptionsAwareHTTPCacheCallback(const RewriteOptions* rewrite_options);

 private:
  int64 cache_invalidation_timestamp_ms_;
  DISALLOW_COPY_AND_ASSIGN(OptionsAwareHTTPCacheCallback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
