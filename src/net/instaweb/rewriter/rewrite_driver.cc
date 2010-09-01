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

#include <vector>
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/base_tag_filter.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/img_rewrite_filter.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/outline_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace {

const char kCssCombiner[] = "cc";
const char kCacheExtender[] = "ce";
const char kFileSystem[] = "fs";
const char kImageCompression[] = "ic";
const char kJavascriptMin[] = "jm";

}  // namespace

namespace net_instaweb {

// TODO(jmarantz): Simplify the interface so we can just use
// asynchronous fetchers, employing FakeUrlAsyncFetcher as needed
// for running functional regression-tests where we don't mind blocking
// behavior.
RewriteDriver::RewriteDriver(MessageHandler* message_handler,
                             FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher)
    : html_parse_(message_handler),
      file_system_(file_system),
      url_async_fetcher_(url_async_fetcher),
      resource_manager_(NULL),
      resource_fetches_(NULL),
      outline_threshold_(0) {
}

RewriteDriver::~RewriteDriver() {
  STLDeleteContainerPointers(filters_.begin(), filters_.end());
}

void RewriteDriver::SetResourceManager(ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;
  html_parse_.set_timer(resource_manager->timer());
}

namespace {

class ContainmentChecker {
 public:
  explicit ContainmentChecker(const StringSet& strings) : strings_(strings) {}
  bool contains(const std::string& str) {
    return strings_.find(str) != strings_.end();
  }
 private:
  const StringSet& strings_;
};

}  // namespace

void RewriteDriver::AddFiltersByCommaSeparatedList(const StringPiece& filters) {
  StringSet filter_set;
  std::vector<StringPiece> names;
  SplitStringPieceToVector(filters, ",", &names, true);
  for (int i = 0, n = names.size(); i < n; ++i) {
    filter_set.insert(std::string(names[i].data(), names[i].size()));
  }
  AddFilters(filter_set);
}

// TODO(jmarantz): validate the set of enabled_features to make sure
// that no invalid ones are specified.
void RewriteDriver::AddFilters(const StringSet& enabled_filters) {
  CHECK(html_writer_filter_ == NULL);
  ContainmentChecker enabled(enabled_filters);
  if (enabled.contains("add_head") ||
      enabled.contains("add_base_tag") ||
      enabled.contains("move_css_to_head")) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddFilter(new AddHeadFilter(&html_parse_));
  }
  if (enabled.contains("add_base_tag")) {
    // Adds a filter that establishes a base tag for the HTML document.
    // This is required when implementing a proxy server.  The base
    // tag used can be changed for every request with SetBaseUrl.
    // Adding the base-tag filter will establish the AddHeadFilter
    // if needed.
    base_tag_filter_.reset(new BaseTagFilter(&html_parse_));
    html_parse_.AddFilter(base_tag_filter_.get());
  }
  if (enabled.contains("strip_scripts")) {
    // Experimental filter that blindly scripts all strips from a page.
    AddFilter(new StripScriptsFilter(&html_parse_));
  }
  if (enabled.contains("outline_css") ||
      enabled.contains("outline_javascript")) {
    // Cut out inlined styles and scripts and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    OutlineFilter* outline_filter =
        new OutlineFilter(&html_parse_, resource_manager_,
                          enabled.contains("outline_css"),
                          enabled.contains("outline_javascript"));
    outline_filter->set_size_threshold_bytes(outline_threshold_);
    AddFilter(outline_filter);
  }
  if (enabled.contains("move_css_to_head")) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AddFilter(new CssMoveToHeadFilter(&html_parse_, statistics()));
  }
  if (enabled.contains("combine_css")) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a resource_manager to be set.
    AddRewriteFilter(new CssCombineFilter(this, kCssCombiner));
  }
  if (enabled.contains("rewrite_images")) {
    AddRewriteFilter(new ImgRewriteFilter(this, kImageCompression));
  }
  if (enabled.contains("rewrite_javascript")) {
    // Rewrite (minify etc.) JavaScript code to reduce time to first
    // interaction.
    AddRewriteFilter(new JavascriptFilter(this, kJavascriptMin));
  }
  if (enabled.contains("remove_comments")) {
    AddFilter(new RemoveCommentsFilter(&html_parse_));
  }
  if (enabled.contains("collapse_whitespace")) {
    // Remove excess whitespace in HTML
    AddFilter(new CollapseWhitespaceFilter(&html_parse_));
  }
  if (enabled.contains("elide_attributes")) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AddFilter(new ElideAttributesFilter(&html_parse_));
  }
  if (enabled.contains("extend_cache")) {
    // Extend the cache lifetime of resources.
    AddRewriteFilter(new CacheExtender(this, kCacheExtender));
  }
  if (enabled.contains("left_trim_urls")) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    left_trim_filter_.reset(
        new UrlLeftTrimFilter(&html_parse_,
                              resource_manager_->statistics()));
    html_parse_.AddFilter(left_trim_filter_.get());
  }
  if (enabled.contains("remove_quotes")) {
    // Remove extraneous quotes from html attributes.  Does this save
    // enough bytes to be worth it after compression?  If we do it
    // everywhere it seems to give a small savings.
    AddFilter(new HtmlAttributeQuoteRemoval(&html_parse_));
  }
}

void RewriteDriver::SetBaseUrl(const StringPiece& base) {
  if (base_tag_filter_ != NULL) {
    base_tag_filter_->set_base_url(base);
  }
  if (left_trim_filter_ != NULL) {
    left_trim_filter_->AddBaseUrl(base);
  }
  if (resource_manager_ != NULL) {
    resource_manager_->set_base_url(base);
  }
}

void RewriteDriver::AddFilter(HtmlFilter* filter) {
  filters_.push_back(filter);
  html_parse_.AddFilter(filter);
}

void RewriteDriver::AddRewriteFilter(RewriteFilter* filter) {
  // Track resource_fetches if we care about statistics.  Note that
  // the statistics are owned by the resource manager, which generally
  // should be set up prior to the rewrite_driver.
  Statistics* stats = statistics();
  if ((stats != NULL) && (resource_fetches_ == NULL)) {
    resource_fetches_ = stats->AddVariable("resource_fetches");
  }
  resource_filter_map_[filter->id()] = filter;
  AddFilter(filter);
}

void RewriteDriver::SetWriter(Writer* writer) {
  if (html_writer_filter_ == NULL) {
    html_writer_filter_.reset(new HtmlWriterFilter(&html_parse_));
    html_parse_.AddFilter(html_writer_filter_.get());
  }
  html_writer_filter_->set_writer(writer);
}

Statistics* RewriteDriver::statistics() const {
  return (resource_manager_ == NULL) ? NULL : resource_manager_->statistics();
}

namespace {

// Wraps an async fetcher callback, in order to delete the output
// resource.
class ResourceDeleterCallback : public UrlAsyncFetcher::Callback {
 public:
  ResourceDeleterCallback(OutputResource* output_resource,
                          UrlAsyncFetcher::Callback* callback)
      : output_resource_(output_resource),
        callback_(callback) {
  }

  virtual void Done(bool status) {
    callback_->Done(status);
    delete this;
  }

 private:
  scoped_ptr<OutputResource> output_resource_;
  UrlAsyncFetcher::Callback* callback_;
};

}  // namespace

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
    const StringPiece& id = components[0];
    const StringPiece& hash = components[1];
    const StringPiece& name = components[2];
    const StringPiece& ext = components[3];

    OutputResource* output_resource = resource_manager_->
        CreateUrlOutputResource(id, name, hash, content_type);
    std::string resource_name;
    resource_manager_->filename_encoder()->Encode(
        resource_manager_->filename_prefix(), resource, &resource_name);

    // strcasecmp is needed for this check because we will canonicalize
    // file extensions based on the table in util/content_type.cc.
    CHECK(strcasecmp(resource_name.c_str(),
                     output_resource->filename().c_str()) == 0);

    callback = new ResourceDeleterCallback(output_resource, callback);
    if (resource_manager_->FetchOutputResource(
            output_resource, writer, response_headers, message_handler)) {
      callback->Done(true);
      queued = true;
    } else {
      StringFilterMap::iterator p = resource_filter_map_.find(
          std::string(id.data(), id.size()));
      if (p != resource_filter_map_.end()) {
        RewriteFilter* filter = p->second;
        std::string resource_ext = StrCat(name, ".", ext);
        queued = filter->Fetch(output_resource, writer,
                               request_headers, response_headers,
                               url_async_fetcher_, message_handler, callback);
        if (resource_fetches_ != NULL) {
          resource_fetches_->Add(1);
        }
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
