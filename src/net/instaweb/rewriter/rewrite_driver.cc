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

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/request_headers.h"
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
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

// RewriteFilter prefixes
const char RewriteDriver::kCssCombinerId[] = "cc";
const char RewriteDriver::kCssFilterId[] = "cf";
const char RewriteDriver::kCacheExtenderId[] = "ce";
const char RewriteDriver::kImageCompressionId[] = "ic";
const char RewriteDriver::kJavascriptMinId[] = "jm";

// TODO(jmarantz): Simplify the interface so we can just use
// asynchronous fetchers, employing FakeUrlAsyncFetcher as needed
// for running functional regression-tests where we don't mind blocking
// behavior.
RewriteDriver::RewriteDriver(MessageHandler* message_handler,
                             FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher,
                             const RewriteOptions& options)
    : html_parse_(message_handler),
      file_system_(file_system),
      url_async_fetcher_(url_async_fetcher),
      resource_manager_(NULL),
      add_instrumentation_filter_(NULL),
      cached_resource_fetches_(NULL),
      succeeded_filter_resource_fetches_(NULL),
      failed_filter_resource_fetches_(NULL),
      options_(options) {
}

RewriteDriver::~RewriteDriver() {
  STLDeleteElements(&filters_);
}

const char* RewriteDriver::kPassThroughRequestAttributes[3] = {
  HttpAttributes::kIfModifiedSince,
  HttpAttributes::kReferer,
  HttpAttributes::kUserAgent
};

// names for Statistics variables.
const char RewriteDriver::kResourceFetchesCached[] = "resource_fetches_cached";
const char RewriteDriver::kResourceFetchConstructSuccesses[] =
    "resource_fetch_construct_successes";
const char RewriteDriver::kResourceFetchConstructFailures[] =
    "resource_fetch_construct_failures";

void RewriteDriver::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(kResourceFetchesCached);
    statistics->AddVariable(kResourceFetchConstructSuccesses);
    statistics->AddVariable(kResourceFetchConstructFailures);

    // TODO(jmarantz): Make all of these work with null statistics so that
    // they could mdo other required static initializations if desired
    // without having to edit code to this method.
    AddInstrumentationFilter::Initialize(statistics);
    CacheExtender::Initialize(statistics);
    CssCombineFilter::Initialize(statistics);
    CssMoveToHeadFilter::Initialize(statistics);
    ImgRewriteFilter::Initialize(statistics);
    JavascriptFilter::Initialize(statistics);
    ResourceManager::Initialize(statistics);
    UrlLeftTrimFilter::Initialize(statistics);
  }
  CssFilter::Initialize(statistics);
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

void RewriteDriver::AddFilters() {
  CHECK(html_writer_filter_ == NULL);

  // Add the rewriting filters to the map unconditionally -- we may
  // need the to process resource requests due to a query-specific
  // 'rewriters' specification.  We still use the passed-in options
  // to determine whether they get added to the html parse filter chain.
  RegisterRewriteFilter(new CssCombineFilter(this, kCssCombinerId));
  RegisterRewriteFilter(new CssFilter(this, kCssFilterId));
  RegisterRewriteFilter(new JavascriptFilter(this, kJavascriptMinId));
  RegisterRewriteFilter(
      new ImgRewriteFilter(
          this,
          options_.Enabled(RewriteOptions::kDebugLogImgTags),
          options_.Enabled(RewriteOptions::kInsertImgDimensions),
          kImageCompressionId,
          options_.img_inline_max_bytes(),
          options_.img_max_rewrites_at_once()));
  RegisterRewriteFilter(new CacheExtender(this, kCacheExtenderId));

  // This function defines the order that filters are run.  We document
  // in pagespeed.conf.template that the order specified in the conf
  // file does not matter, but we give the filters there in the order
  // they are actually applied, for the benefit of the understanding
  // of the site owner.  So if you change that here, change it in
  // install/common/pagespeed.conf.template as well.

  // Now process boolean options, which may include propagating non-boolean
  // and boolean parameter settings to filters.
  if (options_.Enabled(RewriteOptions::kAddHead) ||
      options_.Enabled(RewriteOptions::kCombineHeads) ||
      options_.Enabled(RewriteOptions::kAddBaseTag) ||
      options_.Enabled(RewriteOptions::kMoveCssToHead) ||
      options_.Enabled(RewriteOptions::kAddInstrumentation)) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddFilter(new AddHeadFilter(
        &html_parse_, options_.Enabled(RewriteOptions::kCombineHeads)));
  }
  if (options_.Enabled(RewriteOptions::kAddBaseTag)) {
    // Adds a filter that establishes a base tag for the HTML document.
    // This is required when implementing a proxy server.  The base
    // tag used can be changed for every request with SetBaseUrl.
    // Adding the base-tag filter will establish the AddHeadFilter
    // if needed.
    base_tag_filter_.reset(new BaseTagFilter(&html_parse_));
    html_parse_.AddFilter(base_tag_filter_.get());
  }
  if (options_.Enabled(RewriteOptions::kStripScripts)) {
    // Experimental filter that blindly scripts all strips from a page.
    AddFilter(new StripScriptsFilter(&html_parse_));
  }
  if (options_.Enabled(RewriteOptions::kOutlineCss)) {
    // Cut out inlined styles and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    CssOutlineFilter* css_outline_filter = new CssOutlineFilter(this);
    AddFilter(css_outline_filter);
  }
  if (options_.Enabled(RewriteOptions::kOutlineJavascript)) {
    // Cut out inlined scripts and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    JsOutlineFilter* js_outline_filter = new JsOutlineFilter(this);
    AddFilter(js_outline_filter);
  }
  if (options_.Enabled(RewriteOptions::kMoveCssToHead)) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AddFilter(new CssMoveToHeadFilter(&html_parse_, statistics()));
  }
  if (options_.Enabled(RewriteOptions::kCombineCss)) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a resource_manager to be set.
    EnableRewriteFilter(kCssCombinerId);
  }
  if (options_.Enabled(RewriteOptions::kRewriteCss)) {
    EnableRewriteFilter(kCssFilterId);
  }
  if (options_.Enabled(RewriteOptions::kRewriteJavascript)) {
    // Rewrite (minify etc.) JavaScript code to reduce time to first
    // interaction.
    EnableRewriteFilter(kJavascriptMinId);
  }
  if (options_.Enabled(RewriteOptions::kInlineCss)) {
    // Inline small CSS files.  Give CssCombineFilter and CSS minification a
    // chance to run before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddFilter(new CssInlineFilter(this));
  }
  if (options_.Enabled(RewriteOptions::kInlineJavascript)) {
    // Inline small Javascript files.  Give JS minification a chance to run
    // before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddFilter(new JsInlineFilter(this));
  }
  if (options_.Enabled(RewriteOptions::kRewriteImages)) {
    EnableRewriteFilter(kImageCompressionId);
  }
  if (options_.Enabled(RewriteOptions::kRemoveComments)) {
    AddFilter(new RemoveCommentsFilter(&html_parse_));
  }
  if (options_.Enabled(RewriteOptions::kCollapseWhitespace)) {
    // Remove excess whitespace in HTML
    AddFilter(new CollapseWhitespaceFilter(&html_parse_));
  }
  if (options_.Enabled(RewriteOptions::kElideAttributes)) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AddFilter(new ElideAttributesFilter(&html_parse_));
  }
  if (options_.Enabled(RewriteOptions::kExtendCache)) {
    // Extend the cache lifetime of resources.
    EnableRewriteFilter(kCacheExtenderId);
  }
  if (options_.Enabled(RewriteOptions::kLeftTrimUrls)) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    left_trim_filter_.reset(
        new UrlLeftTrimFilter(&html_parse_,
                              resource_manager_->statistics()));
    html_parse_.AddFilter(left_trim_filter_.get());
  }
  if (options_.Enabled(RewriteOptions::kRemoveQuotes)) {
    // Remove extraneous quotes from html attributes.  Does this save
    // enough bytes to be worth it after compression?  If we do it
    // everywhere it seems to give a small savings.
    AddFilter(new HtmlAttributeQuoteRemoval(&html_parse_));
  }
  if (options_.Enabled(RewriteOptions::kAddInstrumentation)) {
    // Inject javascript to instrument loading-time.
    add_instrumentation_filter_ =
        new AddInstrumentationFilter(&html_parse_,
                                     options_.beacon_url(),
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

void RewriteDriver::RegisterRewriteFilter(RewriteFilter* filter) {
  // Track resource_fetches if we care about statistics.  Note that
  // the statistics are owned by the resource manager, which generally
  // should be set up prior to the rewrite_driver.
  //
  // TODO(sligocki): It'd be nice to get this into the constructor.
  Statistics* stats = statistics();
  if ((stats != NULL) && (cached_resource_fetches_ == NULL)) {
    cached_resource_fetches_ = stats->GetVariable(kResourceFetchesCached);
    succeeded_filter_resource_fetches_ =
        stats->GetVariable(kResourceFetchConstructSuccesses);
    failed_filter_resource_fetches_ =
        stats->GetVariable(kResourceFetchConstructFailures);
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

OutputResource* RewriteDriver::DecodeOutputResource(
    const StringPiece& url,
    RewriteFilter** filter) {
  // Note that this does parsing of the url, but doesn't actually fetch any data
  // until we specifically ask it to.
  OutputResource* output_resource =
      resource_manager_->CreateOutputResourceForFetch(url);

  // If the resource name was ill-formed or unrecognized, we reject the request
  // so it can be passed along.
  if (output_resource != NULL) {
    // For now let's reject as mal-formed if the id string is not
    // in the rewrite drivers.
    //
    // We also reject any unknown extensions, which includes
    // rejecting requests with trailing junk
    //
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
    bool ok = false;
    if (output_resource->type() != NULL) {
      if (p != resource_filter_map_.end()) {
        *filter = p->second;
        ok = true;
      } else if ((id == CssOutlineFilter::kFilterId) ||
                 (id == JsOutlineFilter::kFilterId)) {
        ok = true;
      }
    }

    if (!ok) {
      delete output_resource;
      output_resource = NULL;
      *filter = NULL;
    }
  }
  return output_resource;
}

bool RewriteDriver::FetchResource(
    const StringPiece& url,
    const RequestHeaders& request_headers,
    ResponseHeaders* response_headers,
    Writer* writer,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* callback) {
  bool queued = false;
  bool handled = false;

  // Note that this does permission checking and parsing of the url, but doesn't
  // actually fetch any data until we specifically ask it to.
  RewriteFilter* filter = NULL;
  OutputResource* output_resource = DecodeOutputResource(url, &filter);

  if (output_resource != NULL) {
    handled = true;
    callback = new ResourceDeleterCallback(output_resource, callback,
                                           resource_manager_->http_cache(),
                                           message_handler);
    // None of our resources ever change -- the hash of the content is embedded
    // in the filename.  This is why we serve them with very long cache
    // lifetimes.  However, when the user presses Reload, the browser may
    // attempt to validate that the cached copy is still fresh by sending a GET
    // with an If-Modified-Since header.  If this header is present, we should
    // return a 304 Not Modified, since any representation of the resource
    // that's in the browser's cache must be correct.
    CharStarVector values;
    if (request_headers.Lookup(HttpAttributes::kIfModifiedSince, &values)) {
      response_headers->SetStatusAndReason(HttpStatus::kNotModified);
      callback->Done(true);
      queued = true;
    } else if (resource_manager_->FetchOutputResource(
            output_resource, writer, response_headers, message_handler,
            ResourceManager::kMayBlock)) {
      callback->Done(true);
      queued = true;
      if (cached_resource_fetches_ != NULL) {
        cached_resource_fetches_->Add(1);
      }
    } else if (filter != NULL) {
      queued = filter->Fetch(output_resource, writer,
                             request_headers, response_headers,
                             message_handler, callback);
      if (queued) {
        if (succeeded_filter_resource_fetches_ != NULL) {
          succeeded_filter_resource_fetches_->Add(1);
        }
      } else {
        if (failed_filter_resource_fetches_ != NULL) {
          failed_filter_resource_fetches_->Add(1);
        }
      }
    }
  }
  if (!queued && handled) {
    // If we got here, we were asked to decode a resource for which we have
    // no filter or an invalid URL.
    callback->Done(false);
  }
  return handled;
}

}  // namespace net_instaweb
