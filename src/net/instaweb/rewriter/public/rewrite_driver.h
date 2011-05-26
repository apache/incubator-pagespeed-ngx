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
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

struct ContentType;

class AddInstrumentationFilter;
class CommonFilter;
class DomainRewriteFilter;
class FileSystem;
class HtmlFilter;
class HtmlWriterFilter;
class MessageHandler;
class OutputResource;
class RequestHeaders;
class ResourceContext;
class ResponseHeaders;
class RewriteContext;
class RewriteFilter;
class Statistics;
class Variable;
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

  // Statistics
  static const char kResourceFetchesCached[];
  static const char kResourceFetchConstructSuccesses[];
  static const char kResourceFetchConstructFailures[];

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

  // Clears the current request cache of resources and base URL.  The
  // filter-chain is left intact so that a new request can be issued.
  void Clear();

  // Calls Initialize on all known rewrite_drivers.
  static void Initialize(Statistics* statistics);

  // Adds a resource manager and/or resource_server, enabling the rewriting of
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

  void RememberResource(const StringPiece& url, const ResourcePtr& resource);

  const GoogleString& user_agent() const {
    return user_agent_;
  }
  inline void set_user_agent(const StringPiece& user_agent_string) {
    user_agent_string.CopyToString(&user_agent_);
  }
  const UserAgentMatcher& user_agent_matcher() const {
    return user_agent_matcher_;
  }
  bool UserAgentSupportsImageInlining() const {
    return user_agent_matcher_.SupportsImageInlining(user_agent_);
  }

  // Adds the filters from the options, specified by name in enabled_filters.
  // This must be called explicitly after object construction to provide an
  // opportunity to programatically add custom filters beyond those defined
  // in RewriteOptions, via AddFilter(HtmlFilter* filter) (below).
  void AddFilters();

  // Add any HtmlFilter to the HtmlParse chain and take ownership of the filter.
  void AddOwnedFilter(HtmlFilter* filter);

  // Add a RewriteFilter to the HtmlParse chain and take ownership of the
  // filter.  This differs from AddOwnedFilter in that it adds the filter's ID
  // into a dispatch table for serving rewritten resources.  E.g. if your
  // filter->id == "xy" and FetchResource("NAME.pagespeed.xy.HASH.EXT"...)
  // is called, then RewriteDriver will dispatch to filter->Fetch().
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

  // Attempts to decode an output resource based on the URL pattern
  // without actually rewriting it. No permission checks are performed on the
  // url, though it is parsed to see if it looks like the url of a generated
  // resource (which should mean checking the hash to ensure we generated it
  // ourselves).
  // TODO(jmaessen): add url hash & check thereof.
  OutputResourcePtr DecodeOutputResource(const StringPiece& url,
                                         RewriteFilter** filter);

  FileSystem* file_system() { return file_system_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

  // Determines whether this RewriteDriver was built with custom options
  bool has_custom_options() const { return (custom_options_.get() != NULL); }

  // Takes ownership of 'options'.
  void set_custom_options(RewriteOptions* options) {
    custom_options_.reset(options);
  }

  // Return the options used for this RewriteDriver.
  const RewriteOptions* options() const {
    return (has_custom_options() ? custom_options_.get()
            : resource_manager_->options());
  }

  // Override HtmlParse's StartParseId to propagate any required options.
  virtual bool StartParseId(const StringPiece& url, const StringPiece& id,
                            const ContentType& content_type);

  // Override HtmlParse's FinishParse to ensure that the
  // request-scoped cache is cleared immediately.
  //
  // Note that this method can delete this, if its not externally managed,
  // and if all RewriteContexts have been completed.
  virtual void FinishParse();

  // See comments in resource_manager.h
  OutputResourcePtr CreateOutputResourceFromResource(
      const StringPiece& filter_prefix,
      const UrlSegmentEncoder* encoder,
      const ResourceContext* data,
      const ResourcePtr& input_resource,
      OutputResourceKind kind) {
    return resource_manager_->CreateOutputResourceFromResource(
        options(), filter_prefix, encoder, data, input_resource, kind);
  }

  // See comments in resource_manager.h
  OutputResourcePtr CreateOutputResourceWithPath(
      const StringPiece& path, const StringPiece& filter_prefix,
      const StringPiece& name,  const ContentType* type,
      OutputResourceKind kind) {
    return resource_manager_->CreateOutputResourceWithPath(
        options(), path, filter_prefix, name, type, kind);
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

  // Provides a mechanism for a RewriteContext to notify a
  // RewriteDriver that it is complete, to allow the RewriteDriver
  // to delete itself or return it back to a free pool in the ResourceManager.
  void RewriteComplete(RewriteContext* rewrite_context);

  // If there are not outstanding references to this RewriteDriver,
  // delete it or recycle it to a free pool in the ResourceManager.
  void Cleanup();

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

 private:
  friend class ResourceManagerTestBase;
  friend class ResourceManagerTest;

  typedef std::map<GoogleString, RewriteFilter*> StringFilterMap;
  typedef void (RewriteDriver::*SetStringMethod)(const StringPiece& value);
  typedef void (RewriteDriver::*SetInt64Method)(int64 value);

  // Sets the base GURL in response to a base-tag being parsed.  This
  // should only be called by ScanFilter.
  void SetBaseUrlIfUnset(const StringPiece& new_base);

  // Initializes the base URL at the start of the document.  This is for
  // ScanFilter only.
  void InitBaseUrl();

  // Sets the base URL for a resource fetch.  This should only be called from
  // test code and from FetchResource.
  void SetBaseUrlForFetch(const StringPiece& url);

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
  // html parse filter filter chain.  This allows it to serve resource
  // requests.
  void RegisterRewriteFilter(RewriteFilter* filter);

  // Adds a pre-added rewrite filter to the html parse chain.
  void EnableRewriteFilter(const char* id);

  // Internal low-level helper for resource creation.
  // Use only when permission checking has been done explicitly on the
  // caller side.
  ResourcePtr CreateInputResourceUnchecked(const GoogleUrl& gurl);

  // Attempt to fetch extant version of an OutputResource.  If available,
  // return true. If not, returns false and makes sure the resource is
  // locked for creation. This method may block trying to lock resource
  // for creation, with timeouts of a few seconds.
  // Precondition: output_resource must have a valid URL set (including a hash).
  bool FetchExtantOutputResourceOrLock(
    OutputResource* output_resource,
    Writer* writer, ResponseHeaders* response_headers);

  // Attempt to fetch extant version of an OutputResource, returning true
  // and writing it out to writer and response_headers if available.
  // Does not block or touch resource creation locks.
  bool FetchExtantOutputResource(
    OutputResource* output_resource,
    Writer* writer, ResponseHeaders* response_headers);

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

  GoogleUrl base_url_;
  GoogleString user_agent_;
  StringFilterMap resource_filter_map_;

  typedef std::vector<RewriteContext*> RewriteContextVector;
  RewriteContextVector rewrites_;
  int num_rewrites_complete_;

  // These objects are provided on construction or later, and are
  // owned by the caller.
  FileSystem* file_system_;
  UrlAsyncFetcher* url_async_fetcher_;
  ResourceManager* resource_manager_;

  AddInstrumentationFilter* add_instrumentation_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
  UserAgentMatcher user_agent_matcher_;
  std::vector<HtmlFilter*> filters_;
  ScanFilter scan_filter_;
  scoped_ptr<DomainRewriteFilter> domain_rewriter_;

  // Maps encoded URLs to output URLs
  typedef std::map<GoogleString, ResourcePtr> ResourceMap;
  ResourceMap resource_map_;

  HtmlResourceSlotSet slots_;

  Variable* cached_resource_fetches_;
  Variable* succeeded_filter_resource_fetches_;
  Variable* failed_filter_resource_fetches_;

  scoped_ptr<RewriteOptions> custom_options_;

  // The default resource encoder
  UrlSegmentEncoder default_encoder_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriver);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
