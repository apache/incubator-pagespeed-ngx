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
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/scan_filter.h"
#include "net/instaweb/util/public/abstract_client_state.h"
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
class AsyncFetch;
class CacheUrlAsyncFetcher;
class CollectSubresourcesFilter;
class CommonFilter;
class DomainRewriteFilter;
class FileSystem;
class FlushEarlyInfo;
class Function;
class HtmlEvent;
class HtmlFilter;
class HtmlWriterFilter;
class LogRecord;
class MessageHandler;
class PropertyPage;
class RequestHeaders;
class ResourceContext;
class ResourceNamer;
class ResponseHeaders;
class RewriteContext;
class RewriteFilter;
class ScopedMutex;
class Statistics;
class UrlAsyncFetcher;
class UrlLeftTrimFilter;
class UserAgentMatcher;
class Writer;

// TODO(jmarantz): rename this class to RequestContext.  This extends
// class HtmlParse (which should renamed HtmlContext) by providing
// context for rewriting resources (css, js, images).
//
// Also note that ResourceManager should be renamed ServerContext, as
// it no longer contains much logic about resources.
class RewriteDriver : public HtmlParse {
 public:
  // Status return-code for ResolveCssUrls.
  enum CssResolutionStatus {
    kWriteFailed,
    kNoResolutionNeeded,
    kSuccess
  };

  // Mode for BoundedWaitForCompletion
  enum WaitMode {
    kNoWait,  // Used internally. Do not pass in.
    kWaitForCompletion,    // wait for everything to complete (up to deadline)
    kWaitForCachedRender,  // wait for at least cached rewrites to complete,
                           // and anything else that finishes within deadline.
    kWaitForShutDown       // Makes sure that all work, including any that's
                           // being done in background, finishes.
  };

  // Lazily-initialized boolean value
  enum LazyBool {
    kNotSet = -1,
    kFalse = 0,
    kTrue = 1
  };

  // Indicates document's mimetype as XHTML, HTML, or is not
  // known/something else.  Note that in Apache we might not know the
  // correct mimetype because a downstream module might change it.
  // It's not clear how likely this is, since mod_rewrite and mod_mime
  // run upstream of mod_pagespeed.  However if anyone sets mimetype
  // via "Header Add", it would affect the Browser's view of the
  // document's mimetype (which is what determines the parsing) but
  // mod_pagespeed would not know.
  //
  // Note that we also have doctype().IsXhtml() but that indicates quirks-mode
  // for CSS, and does not control how the parser parses the document.
  enum XhtmlStatus {
    kXhtmlUnknown,
    kIsXhtml,
    kIsNotXhtml
  };

  // A list of HTTP request headers.  These are the headers which
  // should be passed through from the client request into the
  // ResponseHeaders request_headers sent to the rewrite driver.
  // Headers not in this list will be ignored so there is no need to
  // copy them over.
  // TODO(sligocki): Use these in ProxyInterface flow.
  static const char* kPassThroughRequestAttributes[3];

  // This string identifies, for the PropertyCache, a group of properties
  // that are computed from the DOM, and thus can, if desired, be rewritten
  // on every HTML request.
  static const char kDomCohort[];

  // This proprty is used to store information regarding the subresources
  // associted with the HTML page.
  static const char kSubresourcesPropertyName[];

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

  // Calls Initialize on all filters that need it.
  static void Initialize(Statistics* statistics);

  // Calls Terminate on all filters that need it.
  static void Terminate();

  // Adds a resource manager enabling the rewriting of
  // resources. This will replace any previous resource managers.
  void SetResourceManager(ResourceManager* resource_manager);

  // Returns true if we may cache extend Css, Images, PDFs, or Scripts
  // respectively.
  bool MayCacheExtendCss() const;
  bool MayCacheExtendImages() const;
  bool MayCacheExtendPdfs() const;
  bool MayCacheExtendScripts() const;

  void RememberResource(const StringPiece& url, const ResourcePtr& resource);
  const GoogleString& user_agent() const {
    return user_agent_;
  }
  void set_user_agent(const StringPiece& user_agent_string) {
    user_agent_string.CopyToString(&user_agent_);
    user_agent_is_bot_ = kNotSet;
    user_agent_supports_image_inlining_ = kNotSet;
    user_agent_supports_js_defer_ = kNotSet;
    user_agent_supports_webp_ = kNotSet;
    is_mobile_user_agent_ = kNotSet;
  }

  // Returns true if the request we're rewriting was made using SPDY.
  bool using_spdy() const { return using_spdy_; }
  void set_using_spdy(bool x) { using_spdy_ = x; }

  // Return a mutable pointer to the response headers that filters can update
  // before the first flush.  Returns NULL after Flush has occurred.
  ResponseHeaders* mutable_response_headers() {
    return flush_occurred_ ? NULL : response_headers_;
  }

  // Returns a const version of the ResponseHeaders*, indepdendent of whether
  // Flush has occurred.   Note that ResponseHeaders* may still be NULL if
  // no one has called set_response_headers_ptr.
  //
  // TODO(jmarantz): Change API to require response_headers in StartParse so
  // we can guarantee this is non-null.
  const ResponseHeaders* response_headers() {
    return response_headers_;
  }

  // Set the pointer to the response headers that filters can update
  // before the first flush.  RewriteDriver does NOT take ownership
  // of this memory.
  void set_response_headers_ptr(ResponseHeaders* headers) {
    response_headers_ = headers;
  }

  void set_request_headers(const RequestHeaders* headers) {
    request_headers_ = headers;
  }

  const RequestHeaders* request_headers() const {
    DCHECK(request_headers_ != NULL);
    return request_headers_;
  }

  const UserAgentMatcher& user_agent_matcher() const {
    DCHECK(resource_manager() != NULL);
    return resource_manager()->user_agent_matcher();
  }
  bool UserAgentSupportsImageInlining() const;
  bool UserAgentSupportsJsDefer() const;
  bool UserAgentSupportsWebp() const;
  bool IsMobileUserAgent() const;
  bool UserAgentSupportsFlushEarly() const;

  // Adds the filters from the options, specified by name in enabled_filters.
  // This must be called explicitly after object construction to provide an
  // opportunity to programatically add custom filters beyond those defined
  // in RewriteOptions, via AddFilter(HtmlFilter* filter) (below).
  void AddFilters();

  // Adds a filter to the very beginning of the pre-render chain, taking
  // ownership.  This should only be used for filters that must run before any
  // filter added via PrependOwnedPreRenderFilter.
  void AddOwnedEarlyPreRenderFilter(HtmlFilter* filter);

  // Adds a filter to the beginning of the pre-render chain, taking ownership.
  void PrependOwnedPreRenderFilter(HtmlFilter* filter);
  // Adds a filter to the end of the pre-render chain, taking ownership.
  void AppendOwnedPreRenderFilter(HtmlFilter* filter);

  // Adds a filter to the end of the post-render chain, taking ownership.
  void AddOwnedPostRenderFilter(HtmlFilter* filter);
  // Same, without taking ownership.
  void AddUnownedPostRenderFilter(HtmlFilter* filter);

  // Add a RewriteFilter to the end of the pre-render chain and take ownership
  // of the filter.  This differs from AppendOwnedPreRenderFilter in that
  // it adds the filter's ID into a dispatch table for serving
  // rewritten resources.  E.g. if your filter->id == "xy" and
  // FetchResource("NAME.pagespeed.xy.HASH.EXT"...)  is called, then
  // RewriteDriver will dispatch to filter->Fetch().
  //
  // This is used when the filter being added is not part of the
  // core set built into RewriteDriver and RewriteOptions, such
  // as platform-specific or server-specific filters, or filters
  // invented for unit-testing the framework.
  void AppendRewriteFilter(RewriteFilter* filter);

  // Like AppendRewriteFilter, but adds the filter to the beginning of the
  // pre-render chain.
  void PrependRewriteFilter(RewriteFilter* filter);

  // Controls how HTML output is written.  Be sure to call this last, after
  // all other filters have been established.
  //
  // TODO(jmarantz): fix this in the implementation so that the caller can
  // install filters in any order and the writer will always be last.
  void SetWriter(Writer* writer);

  Writer* writer() const { return writer_; }

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
  bool FetchResource(const StringPiece& url, AsyncFetch* fetch);

  // See FetchResource.  There are two differences:
  //   1. It takes an OutputResource instead of a URL.
  //   2. It returns whether a fetch was queued or not.  This is safe
  //      to ignore because in either case the callback will be called.
  //   3. If 'filter' is NULL then the request only checks cache and
  //      (if enabled) the file system.
  bool FetchOutputResource(const OutputResourcePtr& output_resource,
                           RewriteFilter* filter,
                           AsyncFetch* async_fetch);

  // Attempts to decode an output resource based on the URL pattern
  // without actually rewriting it. No permission checks are performed on the
  // url, though it is parsed to see if it looks like the url of a generated
  // resource (which should mean checking the hash to ensure we generated it
  // ourselves).
  // TODO(jmaessen): add url hash & check thereof.
  OutputResourcePtr DecodeOutputResource(const GoogleUrl& url,
                                         RewriteFilter** filter) const;

  // As above, but does not actually create a resource object,
  // and instead outputs the decoded information into the various out
  // parameters. Returns whether decoding successful or not.
  bool DecodeOutputResourceName(const GoogleUrl& url,
                                ResourceNamer* name_out,
                                OutputResourceKind* kind_out,
                                RewriteFilter** filter_out) const;

  // Decodes the incoming pagespeed url to original url(s).
  bool DecodeUrl(const GoogleUrl& url,
                 StringVector* decoded_urls) const;

  FileSystem* file_system() { return file_system_; }
  UrlAsyncFetcher* async_fetcher() { return url_async_fetcher_; }

  // Set a fetcher that will be used by RewriteDriver for current request
  // only (that is, until Clear()). RewriteDriver will take ownership of this
  // fetcher, and will keep it around until Clear(), even if further calls
  // to this method are made.
  void SetSessionFetcher(UrlAsyncFetcher* f);

  // Creates a cache fetcher that uses the driver's fetcher and its options.
  // Note: this means the driver's fetcher must survive as long as this does.
  CacheUrlAsyncFetcher* CreateCacheFetcher();

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

  // Takes ownership of 'options'.
  void set_custom_options(RewriteOptions* options) {
    set_options(true, options);
  }

  // Takes ownership of 'options'.
  void set_options(bool is_custom, RewriteOptions* options) {
    has_custom_options_ = is_custom;
    options_.reset(options);
  }

  // Determines whether this RewriteDriver was built with custom options.
  bool has_custom_options() const { return has_custom_options_; }

  // Return the options used for this RewriteDriver.
  const RewriteOptions* options() const { return options_.get(); }

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

  // Prevent the EndElementEvent for element from flushing.  If it has already
  // flushed, this has no effect.  Should only be called from an event listener.
  // Useful for giving an active filter time to complete an RPC that provides
  // data to append to element.
  void InhibitEndElement(const HtmlElement* element);

  // Permits the EndElementEvent for element to flush.  If it was not previously
  // prevented from doing so by InhibitEndElement, this has no effect.  Should
  // only be called from an active filter, in coordination with an event
  // listener that called InhibitEndElement.  If we are currently flushing,
  // another flush will be scheduled as soon as this one finishes.  If we are
  // not, another flush will be scheduled immediately.
  void UninhibitEndElement(const HtmlElement* element);

  // Returns true if the EndElementEvent for element is inhibited from flushing.
  bool EndElementIsInhibited(const HtmlElement* element);

  // Will return true if the EndElementEvent of element is inhibited from
  // flushing, and that event determined the size of the current flush.  Will
  // return false if a flush is not currently in progress.
  bool EndElementIsStoppingFlush(const HtmlElement* element);

  // Report error message with description of context's location
  // (such as filenames and line numbers). context may be NULL, in which case
  // the current parse position will be used.
  void InfoAt(const RewriteContext* context,
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
      OutputResourceKind kind);

  // Creates an output resource where the name is provided.  The intent is to
  // be able to derive the content from the name, for example, by encoding
  // URLs and metadata.
  //
  // This method succeeds unless the filename is too long.
  //
  // This name is prepended with path for writing hrefs, and the resulting url
  // is encoded and stored at file_prefix when working with the file system.
  // So hrefs are:
  //    $(PATH)/$(NAME).pagespeed[.$EXPERIMENT].$(FILTER_PREFIX).
  //        $(HASH).$(CONTENT_TYPE_EXT)
  //
  // EXPERIMENT is set only when there is an active furious_spec.
  //
  // Could be private since you should use one of the versions below but put
  // here with the rest like it and for documentation clarity.
  OutputResourcePtr CreateOutputResourceWithPath(
      const StringPiece& mapped_path, const StringPiece& unmapped_path,
      const StringPiece& base_url, const StringPiece& filter_id,
      const StringPiece& name, OutputResourceKind kind);

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths are the same and the base_url is this driver's base_url.
  OutputResourcePtr CreateOutputResourceWithUnmappedPath(
      const StringPiece& path, const StringPiece& filter_id,
      const StringPiece& name, OutputResourceKind kind) {
    return CreateOutputResourceWithPath(path, path, base_url_.AllExceptLeaf(),
                                        filter_id, name, kind);
  }

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths are different and the base_url is this driver's base_url.
  OutputResourcePtr CreateOutputResourceWithMappedPath(
      const StringPiece& mapped_path, const StringPiece& unmapped_path,
      const StringPiece& filter_id, const StringPiece& name,
      OutputResourceKind kind) {
    return CreateOutputResourceWithPath(mapped_path, unmapped_path,
                                        decoded_base_url_.AllExceptLeaf(),
                                        filter_id, name, kind);
  }

  // Version of CreateOutputResourceWithPath where the unmapped and mapped
  // paths and the base url are all the same. FOR TESTS ONLY.
  OutputResourcePtr CreateOutputResourceWithPath(
      const StringPiece& path, const StringPiece& filter_id,
      const StringPiece& name, OutputResourceKind kind) {
    return CreateOutputResourceWithPath(path, path, path, filter_id, name,
                                        kind);
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

  // Checks to see if the input_url has the same origin as and the base url, to
  // make sure we're not fetching from another server. Does not consult the
  // domain lawyer, and is not affected by AddDomain().
  // Precondition: input_url.is_valid()
  bool MatchesBaseUrl(const GoogleUrl& input_url) const;

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

  // Returns the appropriate base gurl to be used for resolving hrefs
  // in the document.  Note that HtmlParse::google_url() is the URL
  // for the HTML file and is used for printing html syntax errors.
  const GoogleUrl& base_url() const { return base_url_; }

  // Returns the decoded version of base_gurl() in case it was encoded by a
  // non-default UrlNamer (for the default UrlNamer this returns the same value
  // as base_url()).  Required when fetching a resource by its encoded name.
  const GoogleUrl& decoded_base_url() const { return decoded_base_url_; }
  StringPiece decoded_base() const { return decoded_base_url_.Spec(); }

  // Quick way to tell if the document url is https (ie was fetched via https).
  bool IsHttps() const { return google_url().SchemeIs("https"); }

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

  // Get/set the charset of the containing HTML page. See scan_filter.cc for
  // an explanation of how this is determined, but NOTE that the determined
  // charset can change as more of the HTML is seen, in particular after a
  // meta tag.
  StringPiece containing_charset() { return containing_charset_; }
  void set_containing_charset(const StringPiece charset) {
    charset.CopyToString(&containing_charset_);
  }

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
  // complete they can be rendered.
  void WaitForCompletion();

  // Wait for outstanding rewrite to complete, including any background
  // work that may be ongoing even after results were reported.
  //
  // Note: while this guarantees that the result of the computation is
  // known, the thread that performed it may still be running for a
  // little bit and accessing the driver.
  void WaitForShutDown();

  // As above, but with a time bound, and taking a mode parameter to decide
  // between WaitForCompletion or WaitForShutDown behavior.
  // If timeout_ms <= 0, no time bound will be used.
  void BoundedWaitFor(WaitMode mode, int64 timeout_ms);

  // If this is set to true, during a Flush of HTML the system will
  // wait for results of all rewrites rather than just waiting for
  // cache lookups and a small deadline. Note, however, that in very
  // rare circumstances some rewrites may still be dropped due to
  // excessive load.
  //
  // Note: reset every time the driver is recycled.
  void set_fully_rewrite_on_flush(bool x) {
    fully_rewrite_on_flush_ = x;
  }

  // If the value of X-PSA-Blocking-Rewrite request header matches the blocking
  // rewrite key, set fully_rewrite_on_flush flag.
  void EnableBlockingRewrite(RequestHeaders* request_headers);

  // Indicate that this RewriteDriver will be explicitly deleted, and
  // thus should not be auto-deleted at the end of the parse.  This is
  // primarily for tests.
  //
  // TODO(jmarantz): Consider phasing this out to make tests behave
  // more like servers.
  void set_externally_managed(bool x) { externally_managed_ = x; }

  // Called by RewriteContext to let RewriteDriver know it will be continuing
  // on the fetch in background, and so it should defer doing full cleanup
  // sequences until DetachedFetchComplete() is called.
  void DetachFetch();

  // Called by RewriteContext when a detached async fetch is complete, allowing
  // the RewriteDriver to be recycled if FetchComplete() got invoked as well.
  void DetachedFetchComplete();

  // Cleans up the driver and any fetch rewrite contexts, unless the fetch
  // rewrite got detached by a call to DetachFetch(), in which case a call to
  // DetachedFetchComplete() must also be performed.
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
  int rewrite_deadline_ms() { return rewrite_deadline_ms_; }

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
  bool flush_requested() const { return flush_requested_; }

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
  // the flush is complete.  The inhibits_mutex_ will be held while the callback
  // is running, so the callback should not attempt to inhibit or uninhibit
  // an element.  Further calls to ParseText should be deferred until the
  // callback is called.
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

  // Rewrites CSS content to absolutify any relative embedded URLs, streaming
  // the results to the writer.  Returns 'false' if the writer returns false
  // or if the content was not rewritten because the domains of the gurl
  // and resolved_base match.
  //
  // input_css_base contains the path where the CSS text came from.
  // output_css_base contains the path where the CSS will be written.
  CssResolutionStatus ResolveCssUrls(const GoogleUrl& input_css_base,
                                     const StringPiece& output_css_base,
                                     const StringPiece& contents,
                                     Writer* writer,
                                     MessageHandler* handler);

  // Determines if an URL relative to the given input_base needs to be
  // absolutified given that it will end up under output_base:
  // - If we are proxying and input_base isn't proxy encoded, then yes.
  // - If we aren't proxying and input_base != output_base, then yes.
  // - If we aren't proxying and the domain lawyer will shard or rewrite
  //   input_base, then yes.
  // If not NULL also set *proxy_mode to whether proxy mode is active or not.
  bool ShouldAbsolutifyUrl(const GoogleUrl& input_base,
                           const GoogleUrl& output_base,
                           bool* proxy_mode) const;

  // Update the PropertyValue named 'property_name' in dom cohort with
  // the value 'property_value'.  It is the responsibility of the client to
  // ensure that property cache and dom cohort are enabled when this function is
  // called.  It is a programming error to call this function when property
  // cache or dom cohort is not available, more so since the value payload has
  // to be serialised before calling this function.  Hence this function will
  // DFATAL if property cache or dom cohort is not available.
  void UpdatePropertyValueInDomCohort(StringPiece property_name,
                                      StringPiece property_value);

  // Sets the pointer to the client state associated with this driver.
  // RewriteDriver takes ownership of the provided AbstractClientState object.
  void set_client_state(AbstractClientState* client_state) {
      client_state_.reset(client_state);
  }

  // Return a pointer to the client state associated with this request.
  // This may be NULL if the request does not have an associated client id, or
  // if the retrieval of client state fails.
  AbstractClientState* client_state() const { return client_state_.get(); }

  void set_client_id(const StringPiece& id) { client_id_ = id.as_string(); }
  const GoogleString& client_id() const { return client_id_; }

  PropertyPage* property_page() const { return property_page_.get(); }
  void set_property_page(PropertyPage* page);  // Takes ownership of page.

  // Used by ImageRewriteFilter for identifying critical images.
  const StringSet* critical_images() const {
    return critical_images_.get();
  }

  // Inserts the critical images present on the requested html page. It takes
  // the ownership of critical_images.
  void set_critical_images(StringSet* critical_images) {
    critical_images_.reset(critical_images);
  }

  // We expect to this method to be called on the HTML parser thread.
  // Returns the number of images whose low quality images are inlined in the
  // html page.
  int num_inline_preview_images() const { return num_inline_preview_images_; }

  // We expect to this method to be called on the HTML parser thread.
  void increment_num_inline_preview_images();

  // Increments the value of pending_async_events_. pending_async_events_ will
  // be incremented whenever an async event wants rewrite driver to be alive
  // upon its completion.
  void increment_async_events_count();

  // Decrements the value of pending_async_events_.
  void decrement_async_events_count();

  bool need_to_store_experiment_data() const {
    return need_to_store_experiment_data_;
  }
  void set_need_to_store_experiment_data(bool x) {
    need_to_store_experiment_data_ = x;
  }

  // Determines whether the document's Content-Type has a mimetype indicating
  // that browsers should parse it as XHTML.
  XhtmlStatus MimeTypeXhtmlStatus();

  void set_flushed_early(bool x) { flushed_early_ = x; }
  bool flushed_early() { return flushed_early_; }

  void set_flushing_early(bool x) { flushing_early_ = x; }
  bool flushing_early() { return flushing_early_; }

  // This method is not thread-safe. Call it only from the html parser thread.
  FlushEarlyInfo* flush_early_info();

  void set_serve_blink_non_critical(bool x) { serve_blink_non_critical_ = x; }
  bool serve_blink_non_critical() const { return serve_blink_non_critical_; }

  void set_is_blink_request(bool x) { is_blink_request_ = x; }
  bool is_blink_request() const { return is_blink_request_; }

  // Determines whether we are currently in Debug mode; meaning that the
  // site owner or user has enabled filter kDebug.
  bool DebugMode() const { return options()->Enabled(RewriteOptions::kDebug); }

  // Saves the origin headers for a request in flush_early_info so that it can
  // be used in subsequent request.
  void SaveOriginalHeaders();

  // log_record() can return NULL.
  LogRecord* log_record() { return log_record_; }
  void set_log_record(LogRecord* l) { log_record_ = l; }

 private:
  friend class ResourceManagerTestBase;
  friend class ResourceManagerTest;

  typedef std::map<GoogleString, RewriteFilter*> StringFilterMap;
  typedef void (RewriteDriver::*SetStringMethod)(const StringPiece& value);
  typedef void (RewriteDriver::*SetInt64Method)(int64 value);

  // Backend for both FetchComplete() and DetachedFetchComplete().
  // If 'signal' is true will wake up those waiting for completion on the
  // scheduler. It assumes that rewrite_mutex() will be held via
  // the lock parameter; and releases it when done.
  void FetchCompleteImpl(bool signal, ScopedMutex* lock);

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

  // Returns true if there is a trailing background portion of a detached
  // rewrite for a fetch going on, even if a preliminary answer has
  // already been given.
  // Must be called with rewrites_mutex_ held.
  bool HaveBackgroundFetchRewrite() const;

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

  // After removing an inhibition, finish the parse if necessary.
  void UninhibitFlushDone(Function* user_callback);

  // Move anything on queue_ after the first inhibited event to deferred_queue_.
  void SplitQueueIfNecessary();

  // Helper function to decode the pagespeed url.
  bool DecodeOutputResourceNameHelper(const GoogleUrl& url,
                                      ResourceNamer* name_out,
                                      OutputResourceKind* kind_out,
                                      RewriteFilter** filter_out,
                                      GoogleString* url_base,
                                      StringVector* urls) const;

  // When HTML parsing is complete, we have learned all we can about
  // the DOM, so immediately write anything required into that Cohort
  // into the page property cache.
  void WriteDomCohortIntoPropertyCache();

  // When HTML parsing is complete, write back client state, if it exists,
  // to the property cache.
  void WriteClientStateIntoPropertyCache();

  void FinalizeFilterLogging();

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

  // The charset of the containing HTML page.
  GoogleString containing_charset_;

  bool filters_added_;
  bool externally_managed_;

  // Indicates that a resource fetch has been dispatched to a RewriteContext,
  // and thus the RewriteDriver should not recycled until that RewriteContext
  // has called FetchComplete().
  bool fetch_queued_;            // protected by rewrite_mutex()

  // Indicates that a RewriteContext handling a fetch has elected to
  // return early with unoptimized results and continue rewriting in the
  // background. In this case, the driver (and the context) will not
  // be released until DetachedFetchComplete() has been called.
  bool fetch_detached_;     // protected by rewrite_mutex()

  // For detached fetches, two things have to finish before we can clean them
  // up: the path that answers quickly, and the background path that finishes
  // up the rewrite and writes into the cache. We need to keep track of them
  // carefully since it's not impossible that the "slow" background path
  // might just finish before the "fast" main path in weird thread schedules.
  // Protected by rewrite_mutex()
  bool detached_fetch_main_path_complete_;
  bool detached_fetch_detached_path_complete_;

  // Indicates that the rewrite driver is currently parsing the HTML,
  // and thus should not be recycled under FinishParse() is called.
  bool parsing_;  // protected by rewrite_mutex()

  // If not kNoWait, indicates that WaitForCompletion or similar method
  // have been called, and an another thread is waiting for us to notify it of
  // everything having been finished in a given mode.
  WaitMode waiting_;  // protected by rewrite_mutex()

  // If this is true, the usual HTML streaming interface will let rendering
  // of every flush window fully complete before proceeding rather than
  // use a deadline. This means rewriting of HTML may be slow, and hence
  // should not be used for online traffic.
  bool fully_rewrite_on_flush_;

  // If this is true, this RewriteDriver should Cleanup() itself when it
  // finishes handling the current fetch.
  bool cleanup_on_fetch_complete_;

  bool flush_requested_;
  bool flush_occurred_;

  // If it is true, then the bytes were flushed before receiving bytes from the
  // origin server.
  bool flushed_early_;
  // If set to true, then we are using this RewriteDriver to flush HTML to the
  // user early. This is only set to true when
  // enable_flush_subresources_experimental is true.
  bool flushing_early_;

  // Set to true if RewriteDriver can be released.
  bool release_driver_;

  scoped_ptr<AbstractMutex> inhibits_mutex_;
  typedef std::set <const HtmlElement*> ConstHtmlElementSet;
  ConstHtmlElementSet end_elements_inhibited_;  // protected by inhibits_mutex_
  HtmlEventList deferred_queue_;                // protected by inhibits_mutex_
  Function* finish_parse_on_hold_;              // protected by inhibits_mutex_
  HtmlEvent* inhibiting_event_;                 // protected by inhibits_mutex_
  bool flush_in_progress_;                      // protected by inhibits_mutex_
  bool uninhibit_reflush_requested_;            // protected by inhibits_mutex_

  // Tracks the number of RewriteContexts that have been completed,
  // but not yet deleted.  Once RewriteComplete has been called,
  // rewrite_context->Propagate() is called to render slots (if not
  // detached) and to queue up activity that must occur prior to the
  // context being deleted: specifically running any successors.
  // After all that occurs, DeleteRewriteContext must be called and
  // that will decrement this counter.
  int rewrites_to_delete_;       // protected by rewrite_mutex()

  // URL of the HTML pages being rewritten in the HTML flow or the
  // of the resource being rewritten in the resource flow.
  GoogleUrl base_url_;

  // In the resource flow, the URL requested may not have the same
  // base as the original resource. decoded_base_url_ stores the base
  // of the original (un-rewritten) resource.
  GoogleUrl decoded_base_url_;

  GoogleString user_agent_;
  // Properties of the user_agent_ that are computed once and cached.
  mutable LazyBool user_agent_is_bot_;
  mutable LazyBool user_agent_supports_image_inlining_;
  mutable LazyBool user_agent_supports_js_defer_;
  mutable LazyBool user_agent_supports_webp_;
  mutable LazyBool is_mobile_user_agent_;
  mutable LazyBool user_agent_supports_flush_early_;

  // If true, request is known to have been made using SPDY.
  bool using_spdy_;

  StringFilterMap resource_filter_map_;

  ResponseHeaders* response_headers_;
  const RequestHeaders* request_headers_;

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

  // The number of async events that have been issued, and not yet completed.
  // This is usually used to make the life of driver longer so that any async
  // event that depends on RewriteDriver will be completed before the driver is
  // released.
  int pending_async_events_;             // protected by rewrite_mutex()

  // These objects are provided on construction or later, and are
  // owned by the caller.
  FileSystem* file_system_;
  ResourceManager* resource_manager_;
  Scheduler* scheduler_;
  UrlAsyncFetcher* default_url_async_fetcher_;  // the fetcher we got at ctor

  // This is the fetcher we use --- it's either the default_url_async_fetcher_,
  // or whatever it was temporarily overridden to by SetSessionFetcher.
  // This is either owned externally or via owned_url_async_fetchers_.
  UrlAsyncFetcher* url_async_fetcher_;

  // A list of all the UrlAsyncFetchers that we own, as set with
  // SetSessionFetcher.
  std::vector<UrlAsyncFetcher*> owned_url_async_fetchers_;

  AddInstrumentationFilter* add_instrumentation_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;

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

  scoped_ptr<RewriteOptions> options_;
  bool has_custom_options_;

  // The default resource encoder
  UrlSegmentEncoder default_encoder_;

  // The first chain of filters called before waiting for Rewrites to complete.
  FilterList early_pre_render_filters_;
  // The second chain of filters called before waiting for Rewrites to complete.
  FilterList pre_render_filters_;

  // A container of filters to delete when RewriteDriver is deleted.  This
  // can include pre_render_filters as well as those added to the post-render
  // chain owned by HtmlParse.
  FilterVector filters_to_delete_;

  QueuedWorkerPool::Sequence* html_worker_;
  QueuedWorkerPool::Sequence* rewrite_worker_;
  QueuedWorkerPool::Sequence* low_priority_rewrite_worker_;

  Writer* writer_;

  // Stores a client identifier associated with this request, if any.
  GoogleString client_id_;

  // Stores the AbstractClientState object associated with the client, if any.
  scoped_ptr<AbstractClientState> client_state_;

  // Stores any cached properties associated with the current URL.
  scoped_ptr<PropertyPage> property_page_;

  // Stores all the critical images for the current URL.
  scoped_ptr<StringSet> critical_images_;

  // Do we need to store experiment data for Furious?
  bool need_to_store_experiment_data_;

  // Memoized computation of whether the current doc has an XHTML mimetype.
  bool xhtml_mimetype_computed_;
  XhtmlStatus xhtml_status_ : 8;

  // Number of images whose low quality images are inlined in the html page by
  // InlinePreviewFilter.
  int num_inline_preview_images_;

  CollectSubresourcesFilter* collect_subresources_filter_;

  scoped_ptr<FlushEarlyInfo> flush_early_info_;

  // When non-cacheable panels are absent, non-critical content is already
  // served in blink flow. This flag indicates whether to serve non-critical
  // from panel_filter / blink_filter or not.
  bool serve_blink_non_critical_;
  // Is this a blink request?
  bool is_blink_request_;

  // Pointer to the base fetch's log record, passed in from ProxyInterface; this
  // may be NULL and accessing it beyond the base fetch's lifespan is unsafe.
  // Reset to NULL in Cleanup()
  LogRecord* log_record_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriver);
};

// Subclass of HTTPCache::Callback that incorporates a given RewriteOptions'
// invalidation policy.
class OptionsAwareHTTPCacheCallback : public HTTPCache::Callback {
 public:
  virtual ~OptionsAwareHTTPCacheCallback();
  virtual bool IsCacheValid(const GoogleString& key,
                            const ResponseHeaders& headers);
  virtual int64 OverrideCacheTtlMs(const GoogleString& key);
 protected:
  // Sub-classes need to ensure that rewrite_options remains valid till
  // Callback::Done finishes.
  explicit OptionsAwareHTTPCacheCallback(const RewriteOptions* rewrite_options);

 private:
  const RewriteOptions* rewrite_options_;

  DISALLOW_COPY_AND_ASSIGN(OptionsAwareHTTPCacheCallback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
