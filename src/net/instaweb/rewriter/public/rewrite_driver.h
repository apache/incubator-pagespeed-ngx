/**
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
#include "net/instaweb/util/public/url_async_fetcher.h"
#include "net/instaweb/util/public/user_agent.h"

namespace net_instaweb {

class AddInstrumentationFilter;
class BaseTagFilter;
class FileSystem;
class Hasher;
class HtmlFilter;
class HtmlParse;
class HtmlWriterFilter;
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
  // TODO(jmarantz): provide string-constants so that callers, in particular,
  // tests, that want to enable a specific pass, can reference these rather
  // than replicating the string literals.  Also provide programmatic mechanism
  // to generate simple and detailed help strings for the user enumerating the
  // names of the filters.
  /*
  static const char kAddHead[];
  static const char kAddBaseTag[];
  static const char kMoveCssToHead[];
  static const char kOutlineCss[];
  static const char kOutlineJavascript[];
  */

  RewriteDriver(MessageHandler* message_handler,
                FileSystem* file_system,
                UrlAsyncFetcher* url_async_fetcher);

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

  // Adds the filters, specified by name in enabled_filters.
  void AddFilters(const RewriteOptions& options);
  void AddFilter(RewriteOptions::Filter filter);

  void AddHead() { AddFilter(RewriteOptions::kAddHead); }

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
  // If the pattern does not match, then false is returned, and the request will
  // have to be 404'd by the server passed to some other software to handle.
  // In this case, the callback will be called with Done(false) immediately
  // before the return.
  bool FetchResource(const StringPiece& resource,
                     const MetaData& request_headers,
                     MetaData* response_headers,
                     Writer* writer,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

  HtmlParse* html_parse() { return &html_parse_; }
  FileSystem* file_system() { return file_system_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }
  Statistics* statistics() const;

  AddInstrumentationFilter* add_instrumentation_filter() {
    return add_instrumentation_filter_;
  }

 private:
  friend class ResourceManagerTestBase;
  typedef std::map<std::string, RewriteFilter*> StringFilterMap;
  typedef void (RewriteDriver::*SetStringMethod)(const StringPiece& value);
  typedef void (RewriteDriver::*SetInt64Method)(int64 value);

  bool ParseKeyString(const StringPiece& key, SetStringMethod m,
                      const std::string& flag);
  bool ParseKeyInt64(const StringPiece& key, SetInt64Method m,
                     const std::string& flag);

  // Adds RewriteFilter to the map, but does not put it in the html parse filter
  // filter chain.  This allows it to serve resource requests.
  void AddRewriteFilter(RewriteFilter* filter);

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

  DISALLOW_COPY_AND_ASSIGN(RewriteDriver);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
