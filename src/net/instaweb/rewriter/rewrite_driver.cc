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
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/base_tag_filter.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/img_rewrite_filter.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/js_inline_filter.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace {

// RewriteFilter prefixes
const char kCssCombiner[] = "cc";
const char kCssFilter[] = "cf";
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
      resource_fetches_(NULL) {
}

RewriteDriver::~RewriteDriver() {
  STLDeleteContainerPointers(filters_.begin(), filters_.end());
}

// names for Statistics variables.
const char RewriteDriver::kResourceFetches[] = "resource_fetches";

void RewriteDriver::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(kResourceFetches);
    AddInstrumentationFilter::Initialize(statistics);
    CacheExtender::Initialize(statistics);
    CssCombineFilter::Initialize(statistics);
    CssFilter::Initialize(statistics);
    CssMoveToHeadFilter::Initialize(statistics);
    ImgRewriteFilter::Initialize(statistics);
    JavascriptFilter::Initialize(statistics);
    UrlLeftTrimFilter::Initialize(statistics);
  }
}

void RewriteDriver::SetResourceManager(ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;
  html_parse_.set_timer(resource_manager->timer());
}

// If flag starts with key (a string ending in "="), call m on the remainder of
// flag (the piece after the "=").  Always returns true if the key matched; m is
// free to complain about invalid input using html_parse_->message_handler().
bool RewriteDriver::ParseKeyString(const StringPiece& key, SetStringMethod m,
                                   const std::string& flag) {
  if (flag.rfind(key.data(), 0, key.size()) == 0) {
    StringPiece sp(flag);
    (this->*m)(flag.substr(key.size()));
    return true;
  } else {
    return false;
  }
}

// If flag starts with key (a string ending in "="), convert rest of flag after
// the "=" to Int64, and call m on it.  Always returns true if the key matched;
// m is free to complain about invalid input using
// html_parse_->message_handler() (failure to parse a number does so and never
// calls m).
bool RewriteDriver::ParseKeyInt64(const StringPiece& key, SetInt64Method m,
                                  const std::string& flag) {
  if (flag.rfind(key.data(), 0, key.size()) == 0) {
    std::string str_value = flag.substr(key.size());
    int64 value;
    if (StringToInt64(str_value, &value)) {
      (this->*m)(value);
    } else {
      html_parse_.message_handler()->Message(
          kError, "'%s': ignoring value (should have been int64) after %s",
          flag.c_str(), key.as_string().c_str());
    }
    return true;
  } else {
    return false;
  }
}

void RewriteDriver::AddFilter(RewriteOptions::Filter filter) {
  RewriteOptions options;
  options.EnableFilter(filter);
  AddFilters(options);
}

void RewriteDriver::AddFilters(const RewriteOptions& options) {
  CHECK(html_writer_filter_ == NULL);

  // Add the rewriting filters to the map unconditionally -- we may
  // need the to process resource requests due to a query-specific
  // 'rewriters' specification.  We still use the passed-in options
  // to determine whether they get added to the html parse filter chain.
  AddRewriteFilter(new CssCombineFilter(this, kCssCombiner));
  AddRewriteFilter(new CssFilter(this, kCssFilter));
  AddRewriteFilter(new JavascriptFilter(this, kJavascriptMin));
  AddRewriteFilter(
      new ImgRewriteFilter(
          this,
          options.Enabled(RewriteOptions::kDebugLogImgTags),
          options.Enabled(RewriteOptions::kInsertImgDimensions),
          kImageCompression,
          options.img_inline_max_bytes()));
  AddRewriteFilter(new CacheExtender(this, kCacheExtender));

  // This function defines the order that filters are run.  We document
  // in pagespeed.conf.template that the order specified in the conf
  // file does not matter, but we give the filters there in the order
  // they are actually applied, for the benefit of the understanding
  // of the site owner.  So if you change that here, change it in
  // install/pagespeed.conf.template as well.

  // Now process boolean options, which may include propagating non-boolean
  // and boolean parameter settings to filters.
  if (options.Enabled(RewriteOptions::kAddHead) ||
      options.Enabled(RewriteOptions::kCombineHeads) ||
      options.Enabled(RewriteOptions::kAddBaseTag) ||
      options.Enabled(RewriteOptions::kMoveCssToHead) ||
      options.Enabled(RewriteOptions::kAddInstrumentation)) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddFilter(new AddHeadFilter(
        &html_parse_, options.Enabled(RewriteOptions::kCombineHeads)));
  }
  if (options.Enabled(RewriteOptions::kAddBaseTag)) {
    // Adds a filter that establishes a base tag for the HTML document.
    // This is required when implementing a proxy server.  The base
    // tag used can be changed for every request with SetBaseUrl.
    // Adding the base-tag filter will establish the AddHeadFilter
    // if needed.
    base_tag_filter_.reset(new BaseTagFilter(&html_parse_));
    html_parse_.AddFilter(base_tag_filter_.get());
  }
  if (options.Enabled(RewriteOptions::kStripScripts)) {
    // Experimental filter that blindly scripts all strips from a page.
    AddFilter(new StripScriptsFilter(&html_parse_));
  }
  if (options.Enabled(RewriteOptions::kOutlineCss)) {
    // Cut out inlined styles and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    CssOutlineFilter* css_outline_filter =
        new CssOutlineFilter(&html_parse_, resource_manager_,
                             options.css_outline_min_bytes());
    AddFilter(css_outline_filter);
  }
  if (options.Enabled(RewriteOptions::kOutlineJavascript)) {
    // Cut out inlined scripts and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    JsOutlineFilter* js_outline_filter =
        new JsOutlineFilter(&html_parse_, resource_manager_,
                            options.js_outline_min_bytes());
    AddFilter(js_outline_filter);
  }
  if (options.Enabled(RewriteOptions::kMoveCssToHead)) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AddFilter(new CssMoveToHeadFilter(&html_parse_, statistics()));
  }
  if (options.Enabled(RewriteOptions::kCombineCss)) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a resource_manager to be set.
    EnableRewriteFilter(kCssCombiner);
  }
  if (options.Enabled(RewriteOptions::kRewriteCss)) {
    EnableRewriteFilter(kCssFilter);
  }
  if (options.Enabled(RewriteOptions::kRewriteJavascript)) {
    // Rewrite (minify etc.) JavaScript code to reduce time to first
    // interaction.
    EnableRewriteFilter(kJavascriptMin);
  }
  if (options.Enabled(RewriteOptions::kInlineCss)) {
    // Inline small CSS files.  Give CssCombineFilter and CSS minification a
    // chance to run before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddFilter(new CssInlineFilter(&html_parse_, resource_manager_,
                                  options.css_inline_max_bytes()));
  }
  if (options.Enabled(RewriteOptions::kInlineJavascript)) {
    // Inline small Javascript files.  Give JS minification a chance to run
    // before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddFilter(new JsInlineFilter(&html_parse_, resource_manager_,
                                 options.js_inline_max_bytes()));
  }
  if (options.Enabled(RewriteOptions::kRewriteImages)) {
    EnableRewriteFilter(kImageCompression);
  }
  if (options.Enabled(RewriteOptions::kRemoveComments)) {
    AddFilter(new RemoveCommentsFilter(&html_parse_));
  }
  if (options.Enabled(RewriteOptions::kCollapseWhitespace)) {
    // Remove excess whitespace in HTML
    AddFilter(new CollapseWhitespaceFilter(&html_parse_));
  }
  if (options.Enabled(RewriteOptions::kElideAttributes)) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AddFilter(new ElideAttributesFilter(&html_parse_));
  }
  if (options.Enabled(RewriteOptions::kExtendCache)) {
    // Extend the cache lifetime of resources.
    EnableRewriteFilter(kCacheExtender);
  }
  if (options.Enabled(RewriteOptions::kLeftTrimUrls)) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    left_trim_filter_.reset(
        new UrlLeftTrimFilter(&html_parse_,
                              resource_manager_->statistics()));
    html_parse_.AddFilter(left_trim_filter_.get());
  }
  if (options.Enabled(RewriteOptions::kRemoveQuotes)) {
    // Remove extraneous quotes from html attributes.  Does this save
    // enough bytes to be worth it after compression?  If we do it
    // everywhere it seems to give a small savings.
    AddFilter(new HtmlAttributeQuoteRemoval(&html_parse_));
  }
  if (options.Enabled(RewriteOptions::kAddInstrumentation)) {
    // Inject javascript to instrument loading-time.
    add_instrumentation_filter_ =
        new AddInstrumentationFilter(&html_parse_,
                                     options.beacon_url(),
                                     resource_manager_->statistics());
    AddFilter(add_instrumentation_filter_);
  }
  // NOTE(abliss): Adding a new filter?  Does it export any statistics?  If it
  // doesn't, it probably should.  If it does, be sure to add it to the
  // Initialize() function above or it will break under Apache!
}

void RewriteDriver::SetBaseUrl(const StringPiece& base) {
  if (base_tag_filter_ != NULL) {
    base_tag_filter_->set_base_url(base);
  }
}

void RewriteDriver::AddFilter(HtmlFilter* filter) {
  filters_.push_back(filter);
  html_parse_.AddFilter(filter);
}

void RewriteDriver::EnableRewriteFilter(const char* id) {
  RewriteFilter* filter = resource_filter_map_[id];
  CHECK(filter);
  html_parse_.AddFilter(filter);
}

void RewriteDriver::AddRewriteFilter(RewriteFilter* filter) {
  // Track resource_fetches if we care about statistics.  Note that
  // the statistics are owned by the resource manager, which generally
  // should be set up prior to the rewrite_driver.
  Statistics* stats = statistics();
  if ((stats != NULL) && (resource_fetches_ == NULL)) {
    resource_fetches_ = stats->GetVariable(kResourceFetches);
  }
  resource_filter_map_[filter->id()] = filter;
  filters_.push_back(filter);
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
                          UrlAsyncFetcher::Callback* callback,
                          HTTPCache* http_cache,
                          MessageHandler* message_handler)
      : output_resource_(output_resource),
        callback_(callback),
        http_cache_(http_cache),
        message_handler_(message_handler) {
  }

  virtual void Done(bool status) {
    callback_->Done(status);
    // Filters should generally write their output to the OutputResource,
    // in which case when they are done we can insert it into the cache.
    // However, not all filters do this yet notably img_rewrite_filter,
    // so check for ContentsValid().
    if (status && output_resource_->ContentsValid()) {
      http_cache_->Put(output_resource_->url(),
                       *output_resource_->metadata(),
                       output_resource_->contents(),
                       message_handler_);
    }
    delete this;
  }

 private:
  scoped_ptr<OutputResource> output_resource_;
  UrlAsyncFetcher::Callback* callback_;
  HTTPCache* http_cache_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDeleterCallback);
};

}  // namespace

bool RewriteDriver::FetchResource(
    const StringPiece& url,
    const MetaData& request_headers,
    MetaData* response_headers,
    Writer* writer,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* callback) {
  bool queued = false;
  bool handled = false;

  // Note that this does permission checking and parsing of the url, but doesn't
  // actually fetch any data until we specifically ask it to.
  OutputResource* output_resource =
      resource_manager_->CreateOutputResourceForFetch(
          url, message_handler);

  // If the resource name was ill-formed or unrecognized, we reject the request
  // so it can be passed along.
  if (output_resource == NULL) {
    return false;
  }

  // For now let's reject as mal-formed if the id string is not
  // in the rewrite drivers.
  // TODO(jmarantz): it might be better to 'handle' requests with known
  // IDs even if that filter is not enabled, rather rejecting the request.
  // TODO(jmarantz): consider query-specific rewrites.  We may need to
  // enable filters for this driver based on the referrer.
  StringPiece id = output_resource->filter_prefix();
  StringFilterMap::iterator p = resource_filter_map_.find(
      std::string(id.data(), id.size()));

  // OutlineFilter is special because it's not a RewriteFilter -- it's
  // just an HtmlFilter, but it does encode rewritten resources that
  // must be served from the cache.
  //
  // TODO(jmarantz): figure out a better way to refactor this.
  // TODO(jmarantz): add a unit-test to show serving outline-filter resources.
  if (((p != resource_filter_map_.end()) ||
       (id == CssOutlineFilter::kFilterId) ||
       (id == JsOutlineFilter::kFilterId)) &&
      output_resource->type() != NULL) {
    handled = true;
    callback = new ResourceDeleterCallback(output_resource, callback,
                                           resource_manager_->http_cache(),
                                           message_handler);
    if (resource_manager_->FetchOutputResource(
            output_resource, writer, response_headers, message_handler)) {
      callback->Done(true);
      queued = true;
    } else if (p != resource_filter_map_.end()) {
      RewriteFilter* filter = p->second;
      queued = filter->Fetch(output_resource, writer,
                             request_headers, response_headers,
                             url_async_fetcher_, message_handler, callback);
      if (resource_fetches_ != NULL) {
        resource_fetches_->Add(1);
      }
    }
  }
  if (!queued) {
    // If we got here, we were asked to decode a resource for which we have
    // no filter.
    callback->Done(false);
  }
  return handled;
}

}  // namespace net_instaweb
