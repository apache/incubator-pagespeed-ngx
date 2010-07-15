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

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include <assert.h>
#include <vector>
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/base_tag_filter.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/img_rewrite_filter.h"
#include "net/instaweb/rewriter/public/outline_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace {

const char kCssCombiner[] = "cc";
const char kCacheExtender[] = "ce";
const char kFileSystem[] = "fs";
const char kImageCompression[] = "ic";

}  // namespace

namespace net_instaweb {

// TODO(jmarantz): Simplify the interface so we can just use
// asynchronous fetchers, employing FakeUrlAsyncFetcher as needed
// for running functional regression-tests where we don't mind blocking
// behavior.
RewriteDriver::RewriteDriver(HtmlParse* html_parse, FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher)
    : html_parse_(html_parse),
      file_system_(file_system),
      url_async_fetcher_(url_async_fetcher),
      resource_manager_(NULL) {
}

RewriteDriver::~RewriteDriver() {
}

void RewriteDriver::SetResourceManager(ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;
}

void RewriteDriver::SetBaseUrl(const StringPiece& base) {
  if (base_tag_filter_ != NULL) {
    base_tag_filter_->set_base_url(base);
  }
  if (resource_manager_ != NULL) {
    resource_manager_->set_base_url(base);
  }
}


void RewriteDriver::AddHead() {
  if (add_head_filter_ == NULL) {
    assert(html_writer_filter_ == NULL);
    add_head_filter_.reset(new AddHeadFilter(html_parse_));
    html_parse_->AddFilter(add_head_filter_.get());
  }
}

void RewriteDriver::AddBaseTagFilter() {
  AddHead();
  if (base_tag_filter_ == NULL) {
    assert(html_writer_filter_ == NULL);
    base_tag_filter_.reset(new BaseTagFilter(html_parse_));
    html_parse_->AddFilter(base_tag_filter_.get());
  }
}

void RewriteDriver::ExtendCacheLifetime(Hasher* hasher, Timer* timer) {
  assert(html_writer_filter_ == NULL);
  assert(resource_manager_ != NULL);
  assert(cache_extender_ == NULL);
  cache_extender_.reset(new CacheExtender(kCacheExtender, html_parse_,
                                          resource_manager_, hasher, timer));
  resource_filter_map_[kCacheExtender] = cache_extender_.get();
  html_parse_->AddFilter(cache_extender_.get());
}

void RewriteDriver::CombineCssFiles() {
  assert(html_writer_filter_ == NULL);
  assert(resource_manager_ != NULL);
  assert(css_combine_filter_.get() == NULL);
  css_combine_filter_.reset(
      new CssCombineFilter(kCssCombiner, html_parse_, resource_manager_));
  resource_filter_map_[kCssCombiner] = css_combine_filter_.get();
  html_parse_->AddFilter(css_combine_filter_.get());
}

void RewriteDriver::OutlineResources(bool outline_styles,
                                     bool outline_scripts) {
  // TODO(sligocki): Use FatalError rather than assert.
  assert(html_writer_filter_ == NULL);
  assert(resource_manager_ != NULL);
  outline_filter_.reset(new OutlineFilter(html_parse_, resource_manager_,
                                          outline_styles, outline_scripts));
  html_parse_->AddFilter(outline_filter_.get());
}

void RewriteDriver::RewriteImages() {
  assert(html_writer_filter_ == NULL);
  assert(resource_manager_ != NULL);
  assert(img_rewrite_filter_ == NULL);
  img_rewrite_filter_.reset(
      new ImgRewriteFilter(kImageCompression, html_parse_,
                           resource_manager_, file_system_));
  resource_filter_map_[kImageCompression] = img_rewrite_filter_.get();
  html_parse_->AddFilter(img_rewrite_filter_.get());
}

void RewriteDriver::RemoveQuotes() {
  assert(html_writer_filter_ == NULL);
  assert(attribute_quote_removal_.get() == NULL);
  attribute_quote_removal_.reset(
      new HtmlAttributeQuoteRemoval(html_parse_));
  html_parse_->AddFilter(attribute_quote_removal_.get());
}

void RewriteDriver::SetWriter(Writer* writer) {
  if (html_writer_filter_ == NULL) {
    html_writer_filter_.reset(new HtmlWriterFilter(html_parse_));
    html_parse_->AddFilter(html_writer_filter_.get());
  }
  html_writer_filter_->set_writer(writer);
}

void RewriteDriver::FetchResource(
    const StringPiece& resource,
    const MetaData& request_headers,
    MetaData* response_headers,
    Writer* writer,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* callback) {
  bool queued = false;
  const char* separator = RewriteFilter::prefix_separator();
  std::vector<StringPiece> components;
  SplitStringPieceToVector(resource, separator, &components, false);
  const ContentType* content_type = NameExtensionToContentType(resource);
  if ((content_type != NULL) && (components.size() == 4)) {
    // For now, ignore the hash, which is in components[1]
    const StringPiece& id = components[0];
    const StringPiece& name = components[2];
    const StringPiece& ext = components[3];

    OutputResource* output_resource = resource_manager_->NamedOutputResource(
        id, name, *content_type);

    if (output_resource->Read(resource, writer, response_headers,
                              message_handler)) {
      callback->Done(true);
      queued = true;
    } else {
      ResourceFilterMap::iterator p = resource_filter_map_.find(id);
      if (p != resource_filter_map_.end()) {
        RewriteFilter* filter = p->second;
        std::string resource_ext = StrCat(name, ".", ext);
        queued = filter->Fetch(output_resource, writer,
                               request_headers, response_headers,
                               url_async_fetcher_, message_handler, callback);
      }
    }
  }
  if (!queued) {
    // If we got here, we were asked to decode a resource for which we have
    // no filter.
    callback->Done(false);
  }
}

}  // namespace net_instaweb
