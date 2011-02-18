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
class BaseTagFilter;
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

class RewriteDriver {
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
  void AddFilter(HtmlFilter* filter);

  // Controls how HTML output is written.  Be sure to call this last, after
  // all other filters have been established.
  //
  // TODO(jmarantz): fix this in the implementation so that the caller can
  // install filters in any order and the writer will always be last.
  void SetWriter(Writer* writer);

  // Sets the url that BaseTagFilter will set as the base for the document.
  // This is an no-op if you haven't added BaseTagFilter.
  // Call this for each new document you are processing or the old values will
  // leak through.
  // Note: Use this only to *change* the base URL, not to note the original
  // base URL.
  // TODO(sligocki): Do we need this? We should have some way of noting the
  // base URL of a site if it is explicitly set.
  void SetBaseUrl(const StringPiece& base);

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
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

  // Attempts to decodes an output resource based on the URL pattern
  // without actually rewriting it.
  OutputResource* DecodeOutputResource(const StringPiece& url,
                                       RewriteFilter** filter);

  HtmlParse* html_parse() { return &html_parse_; }
  FileSystem* file_system() { return file_system_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

  const RewriteOptions* options() { return &options_; }

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

  StringFilterMap resource_filter_map_;

  // These objects are provided on construction or later, and are
  // owned by the caller.
  HtmlParse html_parse_;
  FileSystem* file_system_;
  UrlAsyncFetcher* url_async_fetcher_;
  ResourceManager* resource_manager_;

  AddInstrumentationFilter* add_instrumentation_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
  scoped_ptr<BaseTagFilter> base_tag_filter_;
  scoped_ptr<UrlLeftTrimFilter> left_trim_filter_;
  UserAgent user_agent_;
  std::vector<HtmlFilter*> filters_;

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
