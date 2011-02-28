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
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/user_agent.h"

namespace net_instaweb {

class AddInstrumentationFilter;
class FileSystem;
class Hasher;
class HtmlFilter;
class HtmlParse;
class HtmlWriterFilter;
class Resource;
class ResourceNamer;
class RewriteFilter;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class UrlLeftTrimFilter;
class Variable;
class Writer;

class RewriteDriver : public HtmlParse {
 public:
  static const char kCssCombinerId[];
  static const char kCssFilterId[];
  static const char kCacheExtenderId[];
  static const char kImageCompressionId[];
  static const char kJavascriptMinId[];

  // A list of HTTP request headers.  These are the headers which
  // should be passed through from the client request into the
  // ResponseHeaders request_headers sent to the rewrite driver.
  // Headers not in this list will be ignored so there is no need to
  // copy them over.
  static const char* kPassThroughRequestAttributes[3];

  RewriteDriver(MessageHandler* message_handler,
                FileSystem* file_system,
                UrlAsyncFetcher* url_async_fetcher,
                const RewriteOptions& options);

  // Need explicit destructors to allow destruction of scoped_ptr-controlled
  // instances without propagating the include files.
  ~RewriteDriver();

  // Clears the current request cache of resources and base URL.  The
  // filter-chain is left intact so that a new request can be issued.
  void Clear();

  // Calls Initialize on all known rewrite_drivers.
  static void Initialize(Statistics* statistics);

  // Adds a resource manager and/or resource_server, enabling the rewriting of
  // resources. This will replace any previous resource managers.
  void SetResourceManager(ResourceManager* resource_manager);


  void SetUserAgent(const char* user_agent_string) {
    user_agent_.set_user_agent(user_agent_string);
  }

  const UserAgent& user_agent() const {
    return user_agent_;
  }

  // Adds the filters from the options, specified by name in enabled_filters.
  // This must be called explicitly after object construction to provide an
  // opportunity to programatically add custom filters beyond those defined
  // in RewriteOptions, via AddFilter(HtmlFilter* filter) (below).
  void AddFilters();

  // Add any HtmlFilter to the HtmlParse chain and take ownership of the filter.
  void AddOwnedFilter(HtmlFilter* filter);

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

  // Attempts to decodes an output resource based on the URL pattern
  // without actually rewriting it.
  OutputResource* DecodeOutputResource(const StringPiece& url,
                                       RewriteFilter** filter);

  FileSystem* file_system() { return file_system_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

  const RewriteOptions* options() { return &options_; }

  // Override HTMLParse's FinishParse to ensure that the
  // request-scoped cache is cleared immediately.  Beware calling
  // FinishParse on an HtmlParse object, which, since this is not a
  // virtual method, will fail to call Clear().
  void FinishParse() {
    HtmlParse::FinishParse();
    Clear();
  }

  // Created resources are currently the responsibility of the caller.
  // Ultimately we'd like to move to managing resources in a
  // request-scoped map.  Every time a Create...Resource... method is
  // called, a fresh Resource object is generated (or the creation
  // fails and NULL is returned).  All content_type arguments can be
  // NULL if the content type isn't known or isn't covered by the
  // ContentType library.  Where necessary, the extension is used to
  // infer a content type if one is needed and none is provided.  It
  // is faster and more reliable to provide one explicitly when it is
  // known.

  // Constructs an output resource corresponding to the specified input resource
  // and encoded using the provided encoder.  Assumes permissions checking
  // occurred when the input resource was constructed, and does not do it again.
  // To avoid if-chains, tolerates a NULL input_resource (by returning NULL).
  // TODO(jmaessen, jmarantz): Do we want to permit NULL input_resources here?
  // jmarantz has evinced a distaste.
  OutputResource* CreateOutputResourceFromResource(
      const StringPiece& filter_prefix,
      const ContentType* content_type,
      UrlSegmentEncoder* encoder,
      Resource* input_resource);

  enum OutputResourceKind {
    kRewrittenResource,  // derived from some input resource URL or URLs.
    kOutlinedResource   // derived from page HTML.
  };

  // Creates an output resource where the name is provided by the rewriter.
  // The intent is to be able to derive the content from the name, for example,
  // by encoding URLs and metadata.
  //
  // This method is not dependent on shared persistent storage, and always
  // succeeds.
  //
  // This name is prepended with path for writing hrefs, and the resulting url
  // is encoded and stored at file_prefix when working with the file system.  So
  // hrefs are:
  //    $(PATH)/$(NAME).pagespeed.$(FILTER_PREFIX).$(HASH).$(CONTENT_TYPE_EXT)
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  OutputResource* CreateOutputResourceWithPath(
      const StringPiece& path, const StringPiece& filter_prefix,
      const StringPiece& name,  const ContentType* type,
      OutputResourceKind kind);

  // Creates an input resource with the url evaluated based on input_url
  // which may need to be absolutified relative to base_url.  Returns NULL if
  // the input resource url isn't valid, or can't legally be rewritten in the
  // context of this page.
  Resource* CreateInputResource(const GURL& base_url,
                                const StringPiece& input_url);

  // Create input resource from input_url, if it is legal in the context of
  // base_gurl, and if the resource can be read from cache.  If it's not in
  // cache, initiate an asynchronous fetch so it will be on next access.  This
  // is a common case for filters.
  Resource* CreateInputResourceAndReadIfCached(
      const GURL& base_gurl, const StringPiece& input_url);

  // Create an input resource by decoding output_resource using the given
  // encoder.  Assures legality by explicitly permission-checking the result.
  Resource* CreateInputResourceFromOutputResource(
      UrlSegmentEncoder* encoder,
      OutputResource* output_resource);

  // Creates an input resource from the given absolute url.  Requires that the
  // provided url has been checked, and can legally be rewritten in the current
  // page context.
  Resource* CreateInputResourceAbsoluteUnchecked(
      const StringPiece& absolute_url);

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents is cached, the callback will
  // be called directly, rather than asynchronously.  The resource
  // will be passed to the callback, which will be responsible for
  // ultimately freeing the resource.  The resource will have its
  // contents and headers filled in.
  //
  // The resource can be deleted only after the callback is called.
  void ReadAsync(Resource* resource, Resource::AsyncCallback* callback,
                 MessageHandler* message_handler);

  // Load the resource if it is cached (or if it can be fetched quickly).
  // If not send off an asynchronous fetch and store the result in the cache.
  //
  // Returns true if the resource is loaded.
  //
  // The resource remains owned by the caller.
  bool ReadIfCached(Resource* resource);

  // As above, but distinguishes between unavailable in cache and not found
  HTTPCache::FindResult ReadIfCachedWithStatus(Resource* resource);

  // Attempt to fetch extant version of an OutputResource.  Returns false if the
  // resource must be created by the caller.  If true is returned, the resulting
  // data could still be empty (eg because the resource is being rewritten in
  // another thread, or rewriting results in errors), so
  // output_resource->IsWritten() must be checked if this call succeeds.  When
  // blocking=kNeverBlock (the normal case for rewriting html), the call returns
  // quickly if another thread is rewriting.  When blocking=kMayBlock (the
  // normal case for serving resources), the call blocks until the ongoing
  // rewrite completes, or until the lock times out and can be seized by the
  // serving thread.
  bool FetchOutputResource(
    OutputResource* output_resource,
    Writer* writer, ResponseHeaders* response_headers,
    ResourceManager::BlockingBehavior blocking);

  // Creates a resource based on a URL.  This is used for serving rewritten
  // resources.  No permission checks are performed on the url, though it
  // is parsed to see if it looks like the url of a generated resource (which
  // should mean checking the hash to ensure we generated it ourselves).
  // TODO(jmaessen): add url hash & check thereof.
  OutputResource* CreateOutputResourceForFetch(const StringPiece& url);

 private:
  friend class ResourceManagerTestBase;
  typedef std::map<std::string, RewriteFilter*> StringFilterMap;
  typedef void (RewriteDriver::*SetStringMethod)(const StringPiece& value);
  typedef void (RewriteDriver::*SetInt64Method)(int64 value);

  bool ParseKeyString(const StringPiece& key, SetStringMethod m,
                      const std::string& flag);
  bool ParseKeyInt64(const StringPiece& key, SetInt64Method m,
                     const std::string& flag);

  // Registers RewriteFilter in the map, but does not put it in the
  // html parse filter filter chain.  This allows it to serve resource
  // requests.
  void RegisterRewriteFilter(RewriteFilter* filter);

  // Adds a pre-added rewrite filter to the html parse chain.
  void EnableRewriteFilter(const char* id);

  // Internal low-level helper for resource creation.
  // Use only when permission checking has been done explicitly on the
  // caller side.
  Resource* CreateInputResourceUnchecked(const GURL& gurl);

  StringFilterMap resource_filter_map_;

  // These objects are provided on construction or later, and are
  // owned by the caller.
  FileSystem* file_system_;
  UrlAsyncFetcher* url_async_fetcher_;
  ResourceManager* resource_manager_;

  AddInstrumentationFilter* add_instrumentation_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
  scoped_ptr<UrlLeftTrimFilter> left_trim_filter_;
  UserAgent user_agent_;
  std::vector<HtmlFilter*> filters_;

  // Resource-scoped data: must be cleared by Clear().  Note
  // that these are present in the structure but are not used yet.
  //
  // The keys here are fully qualified URLs.
  typedef std::map<std::string, Resource*> ResourceMap;
  ResourceMap resource_map_;

  // Statistics
  static const char kResourceFetchesCached[];
  static const char kResourceFetchConstructSuccesses[];
  static const char kResourceFetchConstructFailures[];

  Variable* cached_resource_fetches_;
  Variable* succeeded_filter_resource_fetches_;
  Variable* failed_filter_resource_fetches_;

  const RewriteOptions& options_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriver);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
