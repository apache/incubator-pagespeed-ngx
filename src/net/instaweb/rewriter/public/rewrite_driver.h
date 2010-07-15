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
#include "base/scoped_ptr.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

class AddHeadFilter;
class BaseTagFilter;
class CacheExtender;
class CssCombineFilter;
class FileSystem;
class Hasher;
class HtmlAttributeQuoteRemoval;
class HtmlParse;
class HtmlWriterFilter;
class ImgRewriteFilter;
class OutlineFilter;
class ResourceManager;
class RewriteFilter;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

class RewriteDriver {
 public:
  explicit RewriteDriver(HtmlParse* html_parse, FileSystem* file_system,
                         UrlAsyncFetcher* url_async_fetcher);
  ~RewriteDriver();

  // Adds a resource manager and/or resource_server, enabling the rewriting of
  // resources. This will replace any previous resource managers.
  void SetResourceManager(ResourceManager* resource_manager);

  // Adds a filter that adds a 'head' section to html documents if
  // none found prior to the body.
  void AddHead();

  // Adds a filter that establishes a base tag for the HTML document.
  // This is required when implementing a proxy server.  The base
  // tag used can be changed for every request with SetBaseUrl.
  // Adding the base-tag filter will establish the AddHeadFilter
  // if needed.
  void AddBaseTagFilter();

  // Combine CSS files in html document.  This can only be called once and
  // requires a resource_manager to be set.
  void CombineCssFiles();

  // Cut out inlined styles and scripts and make them into external resources.
  // This can only be called once and requires a resource_manager to be set.
  void OutlineResources(bool outline_styles, bool outline_scripts);

  // Rewrite image urls to reduce space usage.
  void RewriteImages();

  // Extend the cache lifetime of resources.  This can only be called once and
  // requires a resource_manager to be set.
  void ExtendCacheLifetime(Hasher* hasher, Timer* timer);

  // Remove extraneous quotes from html attributes.  Does this save enough bytes
  // to be worth it after compression?  If we do it everywhere it seems to give
  // a small savings.
  void RemoveQuotes();

  // Controls how HTML output is written.  Be sure to call this last, after
  // all other filters have been established.
  //
  // TODO(jmarantz): fix this in the implementation so that the caller can
  // install filters in any order and the writer will always be last.
  void SetWriter(Writer* writer);

  // Sets the base url for resolving relative URLs in a document.  This
  // will *not* necessarily add a base-tag filter, but will change
  // it if AddBaseTagFilter has been called to use this base.
  //
  // SetBaseUrl may be called multiple times to change the base url.
  //
  // Neither AddBaseTagFilter or SetResourceManager should be called after this.
  void SetBaseUrl(const StringPiece& base);

  void FetchResource(const StringPiece& resource,
                     const MetaData& request_headers,
                     MetaData* response_headers,
                     Writer* writer,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

  HtmlParse* html_parse() { return html_parse_; }
  void set_async_fetcher(UrlAsyncFetcher* f) { url_async_fetcher_ = f; }

  ResourceManager* resource_manager() const { return resource_manager_; }

 private:
  // Note that the use of StringPiece as the map key here implies that
  // the storage for the keys outlive the map, e.g. they should be
  // string literals.
  typedef std::map<StringPiece, RewriteFilter*> ResourceFilterMap;
  ResourceFilterMap resource_filter_map_;

  // These objects are provided on construction or later, and are
  // owned by the caller.
  HtmlParse* html_parse_;
  FileSystem* file_system_;
  UrlAsyncFetcher* url_async_fetcher_;
  ResourceManager* resource_manager_;

  scoped_ptr<AddHeadFilter> add_head_filter_;
  scoped_ptr<BaseTagFilter> base_tag_filter_;
  scoped_ptr<CssCombineFilter> css_combine_filter_;
  scoped_ptr<OutlineFilter> outline_filter_;
  scoped_ptr<ImgRewriteFilter> img_rewrite_filter_;
  scoped_ptr<CacheExtender> cache_extender_;
  scoped_ptr<HtmlAttributeQuoteRemoval> attribute_quote_removal_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_H_
