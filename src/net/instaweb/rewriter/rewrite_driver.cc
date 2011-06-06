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

#include <utility>  // for std::pair
#include <vector>
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/google_analytics_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/image_combine_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/js_combine_filter.h"
#include "net/instaweb/rewriter/public/js_inline_filter.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/rewriter/public/render_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace {

// TODO(jmarantz): make these changeable from the Factory based on the
// requirements of the testing system and the platform.  This might
// also want to change based on how many Flushes there are, as each
// Flush can potentially add this much more latency.
const int kDebugWaitForRewriteMsPerFlush = 100;
const int kOptWaitForRewriteMsPerFlush = 20;
const int kValgrindWaitForRewriteMsPerFlush = 1000;
const int kTestTimeoutMs = 10000;

}  // namespace

namespace net_instaweb {

// RewriteFilter prefixes
const char RewriteDriver::kCssCombinerId[] = "cc";
const char RewriteDriver::kCssFilterId[] = "cf";
const char RewriteDriver::kCacheExtenderId[] = "ce";
const char RewriteDriver::kImageCombineId[] = "is";
const char RewriteDriver::kImageCompressionId[] = "ic";
const char RewriteDriver::kJavascriptCombinerId[] = "jc";
const char RewriteDriver::kJavascriptMinId[] = "jm";

RewriteDriver::RewriteDriver(MessageHandler* message_handler,
                             FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher)
    : HtmlParse(message_handler),
      base_was_set_(false),
      refs_before_base_(false),
      asynchronous_rewrites_(false),
      filters_added_(false),
      externally_managed_(false),
      fetch_queued_(false),
      waiting_for_completion_(false),
      rewrites_to_delete_(0),
      pending_rewrites_(0),
      file_system_(file_system),
      url_async_fetcher_(url_async_fetcher),
      resource_manager_(NULL),
      add_instrumentation_filter_(NULL),
      scan_filter_(this),
      domain_rewriter_(NULL),
      cached_resource_fetches_(NULL),
      succeeded_filter_resource_fetches_(NULL),
      failed_filter_resource_fetches_(NULL) {
  if (RunningOnValgrind()) {
    rewrite_deadline_ms_ = kValgrindWaitForRewriteMsPerFlush;
  } else {
#ifdef NDEBUG
    rewrite_deadline_ms_ = kOptWaitForRewriteMsPerFlush;
#else
    rewrite_deadline_ms_ = kDebugWaitForRewriteMsPerFlush;
#endif
  }
  // The Scan filter always goes first so it can find base-tags.
  HtmlParse::AddFilter(&scan_filter_);
}

RewriteDriver::~RewriteDriver() {
  STLDeleteElements(&filters_);
  Clear();
}

void RewriteDriver::Clear() {
  base_url_.Clear();
  DCHECK(!base_url_.is_valid());
  resource_map_.clear();
  DCHECK(initiated_rewrites_.empty());
  DCHECK(detached_rewrites_.empty());
  DCHECK(rewrites_.empty());
  DCHECK_EQ(0, rewrites_to_delete_);
  DCHECK_EQ(0, pending_rewrites_);
  DCHECK(!fetch_queued_);
}

// Must be called with rewrite_mutex_ held.
bool RewriteDriver::RewritesComplete() const {
  return ((pending_rewrites_ == 0) && !fetch_queued_ &&
          detached_rewrites_.empty() && (rewrites_to_delete_ == 0));
}

void RewriteDriver::WaitForCompletion() {
  if (asynchronous_rewrites_) {
    ScopedMutex lock(rewrite_mutex_.get());
    waiting_for_completion_ = true;
    while (!RewritesComplete()) {
      if (fetch_queued_) {
        message_handler()->Message(kInfo, "waiting for fetch completion");
      } else {
        message_handler()->Message(
            kInfo, "waiting for %d rewrites to complete",
            static_cast<int>(pending_rewrites_ + detached_rewrites_.size()));
      }
      rewrite_condvar_->TimedWait(kTestTimeoutMs);
      // TODO(jmarantz): Eliminate these LOG(INFO) and/or convert them
      // into message_handler()->Message(kInfo...).
      LOG(INFO) << "timed wait complete";
    }
    waiting_for_completion_ = false;
  }
}

void RewriteDriver::Render() {
  // Note that no actual resource Rewriting can occur until this point
  // is reached, where we initiate all the RewriteContexts.
  DCHECK(initiated_rewrites_.empty());
  int num_rewrites = rewrites_.size();
  DCHECK_EQ(pending_rewrites_, num_rewrites);

  // Copy  all of the RewriteContext* into the initiated_rewrites_ set
  // *before* initiating them, as we are doing this before we lock.
  // The RewriteThread can start mutating the initiated_rewrites_
  // set as soon as one is initiated.
  {
    // If not locked, this WRITE to initiated_rewrites_ can race with
    // locked READs of initiated_rewrites_ in RewriteComplete which
    // runs in the Rewrite thread, and RewritesCompete(), which runs
    // in either thread.  Note that the DCHECK above, of
    // initiated_rewrites_.empty(), is a READ and it's OK to have
    // concurrent READs.
    ScopedMutex lock(rewrite_mutex_.get());
    initiated_rewrites_.insert(rewrites_.begin(), rewrites_.end());
  }
  for (int i = 0; i < num_rewrites; ++i) {
    RewriteContext* rewrite_context = rewrites_[i];
    LOG(INFO) << "Initiating rewrite: " << rewrite_context;
    rewrite_context->Initiate();
  }
  rewrites_.clear();
  {
    ScopedMutex lock(rewrite_mutex_.get());
    DCHECK(!fetch_queued_);
    int completed_rewrites = num_rewrites - pending_rewrites_;
    if (pending_rewrites_ == 0) {
      LOG(INFO) << "All " << completed_rewrites
                << " rewrites complete by the time Render was called";
    } else {
      LOG(INFO) << "waiting for " << pending_rewrites_ << " rewrites";
      rewrite_condvar_->TimedWait(rewrite_deadline_ms_);
      completed_rewrites = num_rewrites - pending_rewrites_;
      LOG(INFO) << "found " << completed_rewrites << " completed rewrites";
    }

    // If the output cache lookup came as a HIT in after the deadline, that
    // means that (a) we can't use the result and (b) we don't need
    // to re-initiate the rewrite since it was in fact in cache.  Hopefully
    // the cache system will respond to HIT by making the next HIT faster
    // so it meets our deadline.  In either case we will track with stats.
    //
    resource_manager_->cached_output_hits()->Add(completed_rewrites);
    resource_manager_->cached_output_missed_deadline()->Add(pending_rewrites_);

    // While new slots are created for distinct HtmlElements, Resources can be
    // shared across multiple slots, via resource_map_.  However, to avoid
    // races between outstanding RewriteContexts, we must create new Resources
    // after each Flush.  Note that we only need to do this if there are
    // outstanding rewrites.
    if (pending_rewrites_ != 0) {
      resource_map_.clear();
      for (RewriteContextSet::iterator p = initiated_rewrites_.begin(),
               e = initiated_rewrites_.end(); p != e; ++p) {
        RewriteContext* rewrite_context = *p;
        detached_rewrites_.insert(rewrite_context);
        --pending_rewrites_;
      }
      DCHECK_EQ(0, pending_rewrites_);
      initiated_rewrites_.clear();
    } else {
      DCHECK(initiated_rewrites_.empty());
    }

    slots_.clear();
  }
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
    CacheExtender::Initialize(statistics);
    CssCombineFilter::Initialize(statistics);
    CssMoveToHeadFilter::Initialize(statistics);
    DomainRewriteFilter::Initialize(statistics);
    GoogleAnalyticsFilter::Initialize(statistics);
    ImageRewriteFilter::Initialize(statistics);
    ImageCombineFilter::Initialize(statistics);
    JavascriptFilter::Initialize(statistics);
    JsCombineFilter::Initialize(statistics);
    UrlLeftTrimFilter::Initialize(statistics);
  }
  CssFilter::Initialize(statistics);
}

void RewriteDriver::SetResourceManager(ResourceManager* resource_manager) {
  DCHECK(resource_manager_ == NULL);
  resource_manager_ = resource_manager;
  rewrite_mutex_.reset(resource_manager_->thread_system()->NewMutex());
  rewrite_condvar_.reset(rewrite_mutex_->NewCondvar());
  set_timer(resource_manager->timer());

  DCHECK(resource_filter_map_.empty());

  // Add the rewriting filters to the map unconditionally -- we may
  // need the to process resource requests due to a query-specific
  // 'rewriters' specification.  We still use the passed-in options
  // to determine whether they get added to the html parse filter chain.
  // Note: RegisterRewriteFilter takes ownership of these filters.
  CacheExtender* cache_extender = new CacheExtender(this, kCacheExtenderId);
  ImageCombineFilter* image_combiner = new ImageCombineFilter(this,
                                                              kImageCombineId);
  ImageRewriteFilter* image_rewriter =
      new ImageRewriteFilter(this, kImageCompressionId);

  RegisterRewriteFilter(new CssCombineFilter(this, kCssCombinerId));
  RegisterRewriteFilter(
      new CssFilter(this, kCssFilterId, cache_extender, image_rewriter,
                    image_combiner));
  RegisterRewriteFilter(new JavascriptFilter(this, kJavascriptMinId));
  RegisterRewriteFilter(new JsCombineFilter(this, kJavascriptCombinerId));
  RegisterRewriteFilter(image_rewriter);
  RegisterRewriteFilter(cache_extender);
  RegisterRewriteFilter(image_combiner);

  // When cache-extending CSS files, we must rewrite the embedded
  // resource URLs (usually images) based on any rewriting rules
  // that we have.
  //
  // Note we set this relationship here unconditionally.  Turning
  // on the extend_cache filter and adding domains to rewrite is
  // done separately.
  domain_rewriter_.reset(new DomainRewriteFilter(this, statistics()));
  cache_extender->set_domain_rewriter(domain_rewriter_.get());
}

// If flag starts with key (a string ending in "="), call m on the remainder of
// flag (the piece after the "=").  Always returns true if the key matched; m is
// free to complain about invalid input using message_handler().
bool RewriteDriver::ParseKeyString(const StringPiece& key, SetStringMethod m,
                                   const GoogleString& flag) {
  if (flag.rfind(key.data(), 0, key.size()) == 0) {
    StringPiece sp(flag);
    (this->*m)(flag.substr(key.size()));
    return true;
  } else {
    return false;
  }
}

// If flag starts with key (a string ending in "="), convert rest of
// flag after the "=" to Int64, and call m on it.  Always returns true
// if the key matched; m is free to complain about invalid input using
// message_handler() (failure to parse a number does so and never
// calls m).
bool RewriteDriver::ParseKeyInt64(const StringPiece& key, SetInt64Method m,
                                  const GoogleString& flag) {
  if (flag.rfind(key.data(), 0, key.size()) == 0) {
    GoogleString str_value = flag.substr(key.size());
    int64 value;
    if (StringToInt64(str_value, &value)) {
      (this->*m)(value);
    } else {
      message_handler()->Message(
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
  CHECK(!filters_added_);
  filters_added_ = true;

  // This function defines the order that filters are run.  We document
  // in pagespeed.conf.template that the order specified in the conf
  // file does not matter, but we give the filters there in the order
  // they are actually applied, for the benefit of the understanding
  // of the site owner.  So if you change that here, change it in
  // install/common/pagespeed.conf.template as well.
  //
  // Also be sure to update the doc in net/instaweb/doc/docs/config_filters.ezt.
  //
  // Now process boolean options, which may include propagating non-boolean
  // and boolean parameter settings to filters.
  const RewriteOptions* rewrite_options = options();
  if (rewrite_options->Enabled(RewriteOptions::kAddHead) ||
      rewrite_options->Enabled(RewriteOptions::kCombineHeads) ||
      rewrite_options->Enabled(RewriteOptions::kMoveCssToHead) ||
      rewrite_options->Enabled(RewriteOptions::kMakeGoogleAnalyticsAsync) ||
      rewrite_options->Enabled(RewriteOptions::kAddInstrumentation)) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddOwnedFilter(new AddHeadFilter(
        this, rewrite_options->Enabled(RewriteOptions::kCombineHeads)));
  }
  if (rewrite_options->Enabled(RewriteOptions::kStripScripts)) {
    // Experimental filter that blindly strips all scripts from a page.
    AddOwnedFilter(new StripScriptsFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kOutlineCss)) {
    // Cut out inlined styles and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    CssOutlineFilter* css_outline_filter = new CssOutlineFilter(this);
    AddOwnedFilter(css_outline_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kOutlineJavascript)) {
    // Cut out inlined scripts and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    JsOutlineFilter* js_outline_filter = new JsOutlineFilter(this);
    AddOwnedFilter(js_outline_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kMoveCssToHead)) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AddOwnedFilter(new CssMoveToHeadFilter(this, statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineCss)) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a resource_manager to be set.
    EnableRewriteFilter(kCssCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteCss)) {
    EnableRewriteFilter(kCssFilterId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kMakeGoogleAnalyticsAsync)) {
    // Converts sync loads of Google Analytics javascript to async loads.
    // This needs to be listed before rewrite_javascript because it injects
    // javascript that has comments and extra whitespace.
    AddOwnedFilter(new GoogleAnalyticsFilter(this, statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteJavascript)) {
    // Rewrite (minify etc.) JavaScript code to reduce time to first
    // interaction.
    EnableRewriteFilter(kJavascriptMinId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineJavascript)) {
    // Combine external JS resources. Done after minification and analytics
    // detection, as it converts script sources into string literals, making
    // them opaque to analysis.
    EnableRewriteFilter(kJavascriptCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineCss)) {
    // Inline small CSS files.  Give CssCombineFilter and CSS minification a
    // chance to run before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddOwnedFilter(new CssInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineJavascript)) {
    // Inline small Javascript files.  Give JS minification a chance to run
    // before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddOwnedFilter(new JsInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineImages) ||
      rewrite_options->Enabled(RewriteOptions::kInsertImageDimensions) ||
      rewrite_options->Enabled(RewriteOptions::kRecompressImages) ||
      rewrite_options->Enabled(RewriteOptions::kResizeImages)) {
    EnableRewriteFilter(kImageCompressionId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRemoveComments)) {
    AddOwnedFilter(new RemoveCommentsFilter(this, rewrite_options));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCollapseWhitespace)) {
    // Remove excess whitespace in HTML
    AddOwnedFilter(new CollapseWhitespaceFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kElideAttributes)) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AddOwnedFilter(new ElideAttributesFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kExtendCache)) {
    // Extend the cache lifetime of resources.
    EnableRewriteFilter(kCacheExtenderId);
  }
  if (rewrite_options->domain_lawyer()->can_rewrite_domains() &&
      rewrite_options->Enabled(RewriteOptions::kRewriteDomains)) {
    // Rewrite mapped domains and shard any resources not otherwise rewritten.
    // We want do do this after all the content-changing rewrites, because they
    // will map & shard as part of their execution.
    //
    // TODO(jmarantz): Consider removing all the domain-mapping functionality
    // from other rewrites and do it exclusively in this filter.  Before we
    // do that we'll need to validate this filter so we can turn it on by
    // default.
    //
    // Note that the "domain_lawyer" filter controls whether we rewrite
    // domains for resources in HTML files.  However, when we cache-extend
    // CSS files, we rewrite the domains in them whether this filter is
    // specified or not.
    HtmlParse::AddFilter(domain_rewriter_.get());
  }
  if (rewrite_options->Enabled(RewriteOptions::kLeftTrimUrls)) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    AddOwnedFilter(new UrlLeftTrimFilter(this, statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kRemoveQuotes)) {
    // Remove extraneous quotes from html attributes.  Does this save
    // enough bytes to be worth it after compression?  If we do it
    // everywhere it seems to give a small savings.
    AddOwnedFilter(new HtmlAttributeQuoteRemoval(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kAddInstrumentation)) {
    // Inject javascript to instrument loading-time.
    add_instrumentation_filter_ = new AddInstrumentationFilter(
        this, rewrite_options->beacon_url());
    AddOwnedFilter(add_instrumentation_filter_);
  }
  if (rewrite_options->Enabled(RewriteOptions::kSpriteImages)) {
    EnableRewriteFilter(kImageCombineId);
  }

  if (asynchronous_rewrites_) {
    AddOwnedFilter(new RenderFilter(this));
  }

  // NOTE(abliss): Adding a new filter?  Does it export any statistics?  If it
  // doesn't, it probably should.  If it does, be sure to add it to the
  // Initialize() function above or it will break under Apache!
}

void RewriteDriver::AddOwnedFilter(HtmlFilter* filter) {
  filters_.push_back(filter);
  HtmlParse::AddFilter(filter);
}

void RewriteDriver::AddCommonFilter(CommonFilter* filter) {
  filters_.push_back(filter);
  HtmlParse::AddFilter(filter);
}

void RewriteDriver::AddRewriteFilter(RewriteFilter* filter) {
  RegisterRewriteFilter(filter);
  EnableRewriteFilter(filter->id().c_str());
}

void RewriteDriver::EnableRewriteFilter(const char* id) {
  RewriteFilter* filter = resource_filter_map_[id];
  CHECK(filter);
  HtmlParse::AddFilter(filter);
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

void RewriteDriver::SetAsynchronousRewrites(bool async_rewrites) {
  if (async_rewrites != asynchronous_rewrites_) {
    asynchronous_rewrites_ = async_rewrites;
    if (filters_added_) {
      if (asynchronous_rewrites_) {
        AddOwnedFilter(new RenderFilter(this));
      } else {
        LOG(DFATAL) << "Cannot disable async behavior after filters are added";
      }
    }
  }
}

void RewriteDriver::SetWriter(Writer* writer) {
  if (html_writer_filter_ == NULL) {
    HtmlWriterFilter* writer_filter = new HtmlWriterFilter(this);
    html_writer_filter_.reset(writer_filter);
    HtmlParse::AddFilter(writer_filter);
    writer_filter->set_case_fold(options()->lowercase_html_names());
  }
  html_writer_filter_->set_writer(writer);
}

Statistics* RewriteDriver::statistics() const {
  return (resource_manager_ == NULL) ? NULL : resource_manager_->statistics();
}

OutputResourcePtr RewriteDriver::DecodeOutputResource(const StringPiece& url,
                                                      RewriteFilter** filter) {
  // First, we can't handle anything that's not a valid URL nor is named
  // properly as our resource.
  GoogleUrl gurl(url);
  if (!gurl.is_valid()) {
    return OutputResourcePtr();
  }

  StringPiece name = gurl.LeafSansQuery();
  ResourceNamer namer;
  if (!namer.Decode(name)) {
    return OutputResourcePtr();
  }

  // URLs without any hash are rejected as well, as they do not produce
  // OutputResources with a computable URL. (We do accept 'wrong' hashes since
  // they could come up legitimately under some asynchrony scenarios)
  if (namer.hash().empty()) {
    return OutputResourcePtr();
  }

  // Now let's reject as mal-formed if the id string is not
  // in the rewrite drivers. Also figure out the filter's preferred
  // resource kind.
  StringPiece id = namer.id();
  OutputResourceKind kind = kRewrittenResource;
  StringFilterMap::iterator p = resource_filter_map_.find(
      GoogleString(id.data(), id.size()));
  bool has_async_flow = false;
  if (p != resource_filter_map_.end()) {
    *filter = p->second;
    if ((*filter)->ComputeOnTheFly()) {
      kind = kOnTheFlyResource;
    }
    has_async_flow = (*filter)->HasAsyncFlow();
  } else if ((id == CssOutlineFilter::kFilterId) ||
              (id == JsOutlineFilter::kFilterId)) {
    // OutlineFilter is special because it's not a RewriteFilter -- it's
    // just an HtmlFilter, but it does encode rewritten resources that
    // must be served from the cache.
    //
    // TODO(jmarantz): figure out a better way to refactor this.
    // TODO(jmarantz): add a unit-test to show serving outline-filter resources.
    kind = kOutlinedResource;
  } else {
    return OutputResourcePtr();
  }

  // The RewriteOptions* is not supplied when creating an output-resource
  // on behalf of a fetch.  This is because that field is only used for
  // domain sharding, which is a rewriting activity, not a fetching
  // activity.
  StringPiece base = gurl.AllExceptLeaf();
  OutputResourcePtr output_resource(new OutputResource(
      resource_manager_, base, namer, NULL, NULL, kind));
  output_resource->set_written_using_rewrite_context_flow(has_async_flow);

  // We also reject any unknown extensions, which includes rejecting requests
  // with trailing junk. We do this now since OutputResource figures out
  // the type for us.
  if (output_resource->type() == NULL) {
    output_resource.clear();
    *filter = NULL;
  }

  return output_resource;
}

namespace {

class FetchCallback : public UrlAsyncFetcher::Callback {
 public:
  FetchCallback(RewriteDriver* driver, UrlAsyncFetcher::Callback* callback)
    : driver_(driver),
      callback_(callback) {
  }
  virtual ~FetchCallback() {}
  virtual void Done(bool success) {
    callback_->Done(success);
    driver_->FetchComplete();
    delete this;
  }

 private:
  RewriteDriver* driver_;
  UrlAsyncFetcher::Callback* callback_;
};

}  // namespace

bool RewriteDriver::FetchResource(
    const StringPiece& url,
    const RequestHeaders& request_headers,
    ResponseHeaders* response_headers,
    Writer* writer,
    UrlAsyncFetcher::Callback* callback) {
  DCHECK(!fetch_queued_);
  bool queued = false;
  DCHECK_EQ(0, pending_rewrites_);
  bool handled = false;

  // Note that this does permission checking and parsing of the url, but doesn't
  // actually fetch any data until we specifically ask it to.
  RewriteFilter* filter = NULL;
  OutputResourcePtr output_resource(DecodeOutputResource(url, &filter));

  if (output_resource.get() != NULL) {
    handled = true;

    // None of our resources ever change -- the hash of the content is embedded
    // in the filename.  This is why we serve them with very long cache
    // lifetimes.  However, when the user presses Reload, the browser may
    // attempt to validate that the cached copy is still fresh by sending a GET
    // with an If-Modified-Since header.  If this header is present, we should
    // return a 304 Not Modified, since any representation of the resource
    // that's in the browser's cache must be correct.
    StringStarVector values;
    if (request_headers.Lookup(HttpAttributes::kIfModifiedSince, &values)) {
      response_headers->SetStatusAndReason(HttpStatus::kNotModified);
      callback->Done(true);
      queued = true;
    } else if (FetchExtantOutputResourceOrLock(
        output_resource.get(), writer, response_headers)) {
      // TODO(jmarantz): this clause needs to go asynchronous, possibly via
      // RewriteContext.
      callback->Done(true);
      queued = true;
      cached_resource_fetches_->Add(1);
    } else if (filter != NULL) {
      SetBaseUrlForFetch(url);
      // The resource is locked for creation by
      // the call to FetchExtantOutputResourceOrLock() above.
      callback = new FetchCallback(this, callback);
      fetch_queued_ = true;
      queued = filter->Fetch(output_resource, writer,
                             request_headers, response_headers,
                             message_handler(), callback);
      if (queued) {
        succeeded_filter_resource_fetches_->Add(1);
      } else {
        failed_filter_resource_fetches_->Add(1);
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

void RewriteDriver::FetchComplete() {
  ScopedMutex lock(rewrite_mutex_.get());
  DCHECK(fetch_queued_);
  fetch_queued_ = false;
  DCHECK_EQ(0, pending_rewrites_);
  rewrite_condvar_->Signal();
  if (asynchronous_rewrites_) {
    CHECK_EQ(1U, rewrites_.size());
    STLDeleteElements(&rewrites_);
  }
}

// TODO(jmarantz): remove writer/response_headers args from this function
// and force caller to pull those directly from output_resource, as that will
// save the effort of copying the headers.
//
// It will also simplify this routine quite a bit.
bool RewriteDriver::FetchExtantOutputResourceOrLock(
    OutputResource* output_resource,
    Writer* writer, ResponseHeaders* response_headers) {
  if (output_resource->kind() == kOnTheFlyResource) {
    return false;
  }

  // 1) See if resource is already cached, if so return it.
  if (FetchExtantOutputResource(output_resource, writer, response_headers)) {
    return true;
  }

  // 2) Grab a lock for creation, blocking for it if needed.
  output_resource->LockForCreation(kMayBlock);

  // 3) See if the resource got created while we were waiting for the lock.
  // (If it did, the lock will get released almost immediately in our caller,
  //  as it will cleanup the resource).
  return FetchExtantOutputResource(output_resource, writer, response_headers);
}

bool RewriteDriver::FetchExtantOutputResource(
    OutputResource* output_resource,
    Writer* writer, ResponseHeaders* response_headers) {
  // TODO(jmarantz): we are making lots of copies of the data.  We should
  // retrieve the data from the cache without copying it.

  // The http_cache is shared between multiple different classes in Instaweb.
  // To avoid colliding hash keys, we use a class-specific prefix.
  //
  // TODO(jmarantz): consider formalizing this in the HTTPCache API and
  // doing the StrCat inside.
  bool ret = false;
  StringPiece content;
  MessageHandler* handler = message_handler();
  ResponseHeaders* meta_data = output_resource->response_headers();
  GoogleString url = output_resource->url();
  HTTPCache* http_cache = resource_manager_->http_cache();
  if ((http_cache->Find(url, &output_resource->value_, meta_data, handler)
          == HTTPCache::kFound) &&
      output_resource->value_.ExtractContents(&content) &&
      writer->Write(content, handler)) {
    output_resource->set_written(true);
    ret = true;
  } else if (output_resource->Load(handler)) {
    // OutputResources can also be loaded while not in cache if
    // store_outputs_in_file_system() is true.
    content = output_resource->contents();
    http_cache->Put(url, meta_data, content, handler);
    ret = writer->Write(content, handler);
  }

  if (ret && (response_headers != meta_data)) {
    response_headers->CopyFrom(*meta_data);
  }
  return ret;
}

bool RewriteDriver::MayRewriteUrl(const GoogleUrl& domain_url,
                                  const GoogleUrl& input_url) const {
  bool ret = false;
  if (domain_url.is_valid()) {
    if (options()->IsAllowed(input_url.Spec())) {
      scoped_ptr<GoogleUrl> resolved_request(new GoogleUrl());
      GoogleString mapped_domain_name;
      // TODO(nforman): MapRequestToDomain() may be heavier-weight than we need.
      // Replace it with something that does less copying.
      if (options()->domain_lawyer()->MapRequestToDomain(
              domain_url, input_url.Spec(), &mapped_domain_name,
              resolved_request.get(), message_handler())) {
        ret = true;
      }
    }
  }
  return ret;
}

ResourcePtr RewriteDriver::CreateInputResource(const GoogleUrl& input_url) {
  ResourcePtr resource;
  bool may_rewrite = false;
  if (base_url_.is_valid()) {
    may_rewrite = MayRewriteUrl(base_url_, input_url);
  } else {
    // Shouldn't happen?
    message_handler()->Message(
        kFatal, "invalid base_url_ for '%s'", input_url.spec_c_str());
    DCHECK(false);
  }
  if (may_rewrite) {
    resource = CreateInputResourceUnchecked(input_url);
  } else if (input_url.SchemeIs("data")) {
    // skip and silently ignore; don't log a failure.
  } else {
    message_handler()->Message(kInfo, "No permission to rewrite '%s'",
                               input_url.spec_c_str());
    resource_manager_->resource_url_domain_rejections()->Add(1);
  }
  return resource;
}

ResourcePtr RewriteDriver::CreateInputResourceAbsoluteUnchecked(
    const StringPiece& absolute_url) {
  GoogleUrl url(absolute_url);
  if (!url.is_valid()) {
    // Note: Bad user-content can leave us here.  But it's really hard
    // to concatenate a valid protocol and domain onto an arbitrary string
    // and end up with an invalid GURL.
    message_handler()->Message(kInfo, "Invalid resource url '%s'",
                               url.spec_c_str());
    return ResourcePtr();
  }
  return CreateInputResourceUnchecked(url);
}

ResourcePtr RewriteDriver::CreateInputResourceUnchecked(const GoogleUrl& url) {
  StringPiece url_string = url.Spec();
  ResourcePtr resource;

  if (url.SchemeIs("data")) {
    resource = DataUrlInputResource::Make(url_string, resource_manager_);
    if (resource.get() == NULL) {
      // Note: Bad user-content can leave us here.
      message_handler()->Message(kWarning, "Badly formatted data url '%s'",
                                 url_string.as_string().c_str());
    }
  } else if (url.SchemeIs("http")) {
    // Note: type may be NULL if url has an unexpected or malformed extension.
    const ContentType* type = NameExtensionToContentType(url_string);
    GoogleString filename;
    if (options()->file_load_policy()->ShouldLoadFromFile(url, &filename)) {
      resource.reset(new FileInputResource(resource_manager_, options(), type,
                                           url_string, filename));
    } else {
      resource.reset(new UrlInputResource(resource_manager_, options(), type,
                                          url_string));
    }
  } else {
    // Note: Valid user-content can leave us here.
    // Specifically, any URLs with scheme other than data: or http:, say https:.
    // TODO(sligocki): Is this true? Or will such URLs not make it this far?
    message_handler()->Message(kWarning, "Unsupported scheme '%s' for url '%s'",
                               url.Scheme().as_string().c_str(),
                               url_string.as_string().c_str());
  }
  return resource;
}

void RewriteDriver::ReadAsync(Resource::AsyncCallback* callback,
                              MessageHandler* handler) {
  // TODO(jmarantz): fix call-sites and eliminate this wrapper.
  resource_manager_->ReadAsync(callback);
}

bool RewriteDriver::ReadIfCached(const ResourcePtr& resource) {
  return (ReadIfCachedWithStatus(resource) == HTTPCache::kFound);
}

HTTPCache::FindResult RewriteDriver::ReadIfCachedWithStatus(
    const ResourcePtr& resource) {
  HTTPCache::FindResult result = HTTPCache::kNotFound;
  MessageHandler* handler = message_handler();

  // If the resource is not already loaded, and this type of resource (e.g.
  // URL vs File vs Data) is cacheable, then try to load it.
  if (resource->loaded()) {
    result = HTTPCache::kFound;
  } else if (resource->IsCacheable()) {
    result = resource_manager_->http_cache()->Find(
        resource->url(), &resource->value_, resource->response_headers(),
        handler);
  }
  if ((result == HTTPCache::kNotFound) && resource->Load(handler)) {
    result = HTTPCache::kFound;
  }
  if (result == HTTPCache::kFound) {
    resource->DetermineContentType();
    resource_manager_->RefreshIfImminentlyExpiring(resource.get(), handler);
  }
  return result;
}

bool RewriteDriver::StartParseId(const StringPiece& url, const StringPiece& id,
                                 const ContentType& content_type) {
  set_log_rewrite_timing(options()->log_rewrite_timing());
  bool ret = HtmlParse::StartParseId(url, id, content_type);
  if (ret) {
    base_was_set_ = false;
    if (is_url_valid()) {
      base_url_.Reset(google_url().AllExceptLeaf());
    }
  }
  return ret;
}

void RewriteDriver::RewriteComplete(RewriteContext* rewrite_context) {
  ScopedMutex lock(rewrite_mutex_.get());
  DCHECK(!fetch_queued_);
  bool signal = false;
  bool attached = false;
  RewriteContextSet::iterator p = initiated_rewrites_.find(rewrite_context);
  if (p != initiated_rewrites_.end()) {
    initiated_rewrites_.erase(p);
    attached = true;

    --pending_rewrites_;
    if (pending_rewrites_ == 0) {
      signal = true;
    }
  } else {
    int erased = detached_rewrites_.erase(rewrite_context);
    CHECK_EQ(1, erased) << " rewrite_context " << rewrite_context
                        << " not in either detached_rewrites or "
                        << "initiated_rewrites_";
    if (waiting_for_completion_ && detached_rewrites_.empty()) {
      signal = true;
    }
  }
  LOG(INFO) << "rewrite_context " << rewrite_context << " complete "
            << (attached ? "(attached)" : "(detached)");
  rewrite_context->Propagate(attached);
  ++rewrites_to_delete_;
  if (signal) {
    DCHECK(!fetch_queued_);
    rewrite_condvar_->Signal();
  }
}

void RewriteDriver::DeleteRewriteContext(RewriteContext* rewrite_context) {
  bool ready_to_recycle = false;
  {
    ScopedMutex lock(rewrite_mutex_.get());
    DCHECK_LT(0, rewrites_to_delete_);
    --rewrites_to_delete_;
    delete rewrite_context;
    if (RewritesComplete()) {
      if (waiting_for_completion_) {
        rewrite_condvar_->Signal();
      } else {
        ready_to_recycle = !externally_managed_;
      }
    }
  }
  if (ready_to_recycle) {
    Recycle();
  }
}

void RewriteDriver::Recycle() {
  if (has_custom_options()) {
    delete this;
  } else {
    Clear();
    resource_manager_->ReleaseRewriteDriver(this);
  }
}

void RewriteDriver::Cleanup() {
  if (!externally_managed_) {
    bool done = false;
    {
      ScopedMutex lock(rewrite_mutex_.get());
      done = RewritesComplete();
    }
    if (done) {
      Recycle();
    }
  }
}

void RewriteDriver::FinishParse() {
  HtmlParse::FinishParse();
  Cleanup();
}

void RewriteDriver::SetBaseUrlIfUnset(const StringPiece& new_base) {
  // Base url is relative to the document URL in HTML5, but not in
  // HTML4.01.  FF3.x does it HTML4.01 way, Chrome, Opera 11 and FF4
  // betas do it according to HTML5, as is our implementation here.
  GoogleUrl new_base_url(base_url_, new_base);
  if (new_base_url.is_valid()) {
    if (base_was_set_) {
      if (new_base_url.Spec() != base_url_.Spec()) {
        InfoHere("Conflicting base tags: %s and %s",
                 new_base_url.spec_c_str(),
                 base_url_.spec_c_str());
      }
    } else {
      base_was_set_ = true;
      base_url_.Swap(&new_base_url);
    }
  } else {
    InfoHere("Invalid base tag %s relative to %s",
             new_base.as_string().c_str(),
             base_url_.spec_c_str());
  }
}

void RewriteDriver::SetBaseUrlForFetch(const StringPiece& url) {
  // Set the base url for the resource fetch.  This corresponds to where the
  // fetched resource resides (which might or might not be where the original
  // resource lived).
  if (!base_url_.is_valid()) {
    // TODO(jmaessen): we're re-constructing a GoogleUrl after having already
    // done so (repeatedly over several calls) in DecodeOutputResource!  Gah!
    // We at least assume that base_url_ is valid since it was checked when
    // output_resource was created.
    base_url_.Reset(url);
    DCHECK(base_url_.is_valid());
    base_was_set_ = false;
  }
}

bool RewriteDriver::FindResource(const StringPiece& url,
                                 ResourcePtr* resource) const {
  bool ret = false;
  GoogleString url_str(url.data(), url.size());
  ResourceMap::const_iterator iter = resource_map_.find(url_str);
  if (iter != resource_map_.end()) {
    resource->reset(iter->second);
    ret = true;
  }
  return ret;
}

void RewriteDriver::RememberResource(const StringPiece& url,
                                     const ResourcePtr& resource) {
  GoogleString url_str(url.data(), url.size());
  resource_map_[url_str] = resource;
}

RewriteFilter* RewriteDriver::FindFilter(const StringPiece& id) const {
  RewriteFilter* filter = NULL;
  StringFilterMap::const_iterator p = resource_filter_map_.find(id.as_string());
  if (p != resource_filter_map_.end()) {
    filter = p->second;
  }
  return filter;
}

HtmlResourceSlotPtr RewriteDriver::GetSlot(
    const ResourcePtr& resource, HtmlElement* elt,
    HtmlElement::Attribute* attr) {
  HtmlResourceSlot* slot_obj = new HtmlResourceSlot(resource, elt, attr, this);
  HtmlResourceSlotPtr slot(slot_obj);
  std::pair<HtmlResourceSlotSet::iterator, bool> iter_found =
      slots_.insert(slot);
  if (!iter_found.second) {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in.
    HtmlResourceSlotSet::iterator iter = iter_found.first;
    slot.reset(*iter);
  }
  return slot;
}

void RewriteDriver::InitiateRewrite(RewriteContext* rewrite_context) {
  rewrites_.push_back(rewrite_context);
  ++pending_rewrites_;
}

void RewriteDriver::InitiateFetch(RewriteContext* rewrite_context) {
  // Note that we don't let the fetch start until ::Render(), above,
  // loops through all the rewriters_ and calls Initiate().  This
  // avoids races between rewriters mutating slots, and filters adding
  // new Rewriters with slots.
  DCHECK_EQ(0, pending_rewrites_);
  DCHECK(fetch_queued_);
  rewrites_.push_back(rewrite_context);
}

}  // namespace net_instaweb
