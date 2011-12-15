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

#include <cstdarg>
#include <map>
#include <set>
#include <utility>  // for std::pair
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/bot_checker.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/ajax_rewrite_context.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/div_structure_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/flush_html_filter.h"
#include "net/instaweb/rewriter/public/google_analytics_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/image_combine_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/js_combine_filter.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/js_inline_filter.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/meta_tag_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/scan_filter.h"
#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// TODO(jmarantz): make these changeable from the Factory based on the
// requirements of the testing system and the platform.  This might
// also want to change based on how many Flushes there are, as each
// Flush can potentially add this much more latency.
const int kDebugWaitForRewriteMsPerFlush = 20;
const int kOptWaitForRewriteMsPerFlush = 10;
const int kValgrindWaitForRewriteMsPerFlush = 1000;
const int kTestTimeoutMs = 10000;

}  // namespace

class FileSystem;

RewriteDriver::RewriteDriver(MessageHandler* message_handler,
                             FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher)
    : HtmlParse(message_handler),
      base_was_set_(false),
      refs_before_base_(false),
      filters_added_(false),
      externally_managed_(false),
      fetch_queued_(false),
      fetch_detached_(false),
      detached_fetch_main_path_complete_(false),
      detached_fetch_detached_path_complete_(false),
      waiting_(kNoWait),
      cleanup_on_fetch_complete_(false),
      flush_requested_(false),
      panel_filter_incomplete_(false),
      inhibits_mutex_(NULL),
      finish_parse_on_hold_(NULL),
      inhibiting_event_(NULL),
      flush_in_progress_(false),
      uninhibit_reflush_requested_(false),
      rewrites_to_delete_(0),
      user_agent_is_bot_(kNotSet),
      user_agent_supports_image_inlining_(kNotSet),
      user_agent_supports_webp_(kNotSet),
      response_headers_(NULL),
      pending_rewrites_(0),
      possibly_quick_rewrites_(0),
      file_system_(file_system),
      url_async_fetcher_(url_async_fetcher),
      resource_manager_(NULL),
      scheduler_(NULL),
      add_instrumentation_filter_(NULL),
      scan_filter_(this),
      domain_rewriter_(NULL),
      html_worker_(NULL),
      rewrite_worker_(NULL),
      low_priority_rewrite_worker_(NULL),
      writer_(NULL),
      start_time_ms_(0) {
  // Set up default values for the amount of time an HTML rewrite will wait for
  // Rewrites to complete, based on whether compiled for debug or running on
  // valgrind.  Note that unit-tests can explicitly override this value via
  // set_rewrite_deadline_ms().
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
  pre_render_filters_.push_back(&scan_filter_);
}

RewriteDriver::~RewriteDriver() {
  if (rewrite_worker_ != NULL) {
    scheduler_->UnregisterWorker(rewrite_worker_);
    resource_manager_->rewrite_workers()->FreeSequence(rewrite_worker_);
  }
  if (html_worker_ != NULL) {
    scheduler_->UnregisterWorker(html_worker_);
    resource_manager_->html_workers()->FreeSequence(html_worker_);
  }
  if (low_priority_rewrite_worker_ != NULL) {
    scheduler_->UnregisterWorker(low_priority_rewrite_worker_);
    resource_manager_->low_priority_rewrite_workers()->FreeSequence(
        low_priority_rewrite_worker_);
  }
  STLDeleteElements(&filters_to_delete_);
  Clear();
}

RewriteDriver* RewriteDriver::Clone() {
  RewriteDriver* result;
  if (has_custom_options()) {
    RewriteOptions* options_copy = options()->Clone();
    resource_manager_->ComputeSignature(options_copy);
    result = resource_manager_->NewCustomRewriteDriver(options_copy);
  } else {
    result = resource_manager_->NewRewriteDriver();
  }
  return result;
}

void RewriteDriver::Clear() {
  DCHECK(!flush_requested_);
  cleanup_on_fetch_complete_ = false;
  base_url_.Clear();
  DCHECK(!base_url_.is_valid());
  cookies_.clear();
  decoded_base_url_.Clear();
  resource_map_.clear();
  DCHECK(end_elements_inhibited_.empty());
  DCHECK(deferred_queue_.empty());
  DCHECK(inhibiting_event_ == NULL);
  DCHECK(finish_parse_on_hold_ == NULL);
  DCHECK(!flush_in_progress_);
  DCHECK(!uninhibit_reflush_requested_);
  DCHECK(primary_rewrite_context_map_.empty());
  DCHECK(initiated_rewrites_.empty());
  DCHECK(detached_rewrites_.empty());
  DCHECK(rewrites_.empty());
  DCHECK_EQ(0, rewrites_to_delete_);
  DCHECK_EQ(0, pending_rewrites_);
  DCHECK_EQ(0, possibly_quick_rewrites_);
  DCHECK(!fetch_queued_);
  response_headers_ = NULL;
  fetch_detached_ = false;
  detached_fetch_detached_path_complete_ = false;
  detached_fetch_main_path_complete_ = false;
  panel_filter_incomplete_ = false;
  start_time_ms_ = 0;
}

// Must be called with rewrite_mutex() held.
bool RewriteDriver::RewritesComplete() const {
  return ((pending_rewrites_ == 0) && !fetch_queued_ &&
          detached_rewrites_.empty() && (rewrites_to_delete_ == 0) &&
          !panel_filter_incomplete_);
}

void RewriteDriver::SetPanelFilterIncomplete(bool panel_filter_incomplete) {
  bool clean = false;
  {
    // TODO(jmarantz): We'd like to make 'parsing_' symmetric with the other
    // booleans checked for in RewritesComplete(), but it does not work that
    // way now.  In non-panel-related flows, Cleanup() is used to indicate
    // that parsing is complete, hence parsing_ is set to false *in* Cleanup.
    //
    // I think this code would be cleaner with a refactor toward this symmetry.
    ScopedMutex lock(rewrite_mutex());
    clean = panel_filter_incomplete_ && !panel_filter_incomplete && !parsing_;
    panel_filter_incomplete_ = panel_filter_incomplete;
  }
  if (clean) {
    Cleanup();
  }
}

bool RewriteDriver::HaveBackgroundFetchRewrite() const {
  return (fetch_detached_ &&
          !(detached_fetch_main_path_complete_ &&
            detached_fetch_detached_path_complete_));
}

void RewriteDriver::WaitForCompletion() {
  BoundedWaitFor(kWaitForCompletion, -1);
}

void RewriteDriver::WaitForShutDown() {
  BoundedWaitFor(kWaitForShutDown, -1);
}

void RewriteDriver::BoundedWaitFor(WaitMode mode, int64 timeout_ms) {
  SchedulerBlockingFunction wait(scheduler_);

  {
    ScopedMutex lock(rewrite_mutex());
    CheckForCompletionAsync(mode, timeout_ms, &wait);
  }
  wait.Block();
}

void RewriteDriver::CheckForCompletionAsync(WaitMode wait_mode,
                                            int64 timeout_ms,
                                            Function* done) {
  scheduler_->DCheckLocked();
  DCHECK(wait_mode != kNoWait);
  DCHECK(waiting_ == kNoWait);
  waiting_ = wait_mode;

  int64 end_time_ms;
  if (timeout_ms <= 0) {
    end_time_ms = -1;  // Encodes unlimited
  } else {
    end_time_ms = resource_manager()->timer()->NowMs() + timeout_ms;
  }

  TryCheckForCompletion(wait_mode, end_time_ms, done);
}

void RewriteDriver::TryCheckForCompletion(
    WaitMode wait_mode, int64 end_time_ms, Function* done) {
  scheduler_->DCheckLocked();
  bool deadline_reached;
  int64 now_ms = resource_manager_->timer()->NowMs();
  int64 sleep_ms;
  if (end_time_ms < 0) {
    deadline_reached = false;  // Unlimited wait..
    sleep_ms = kTestTimeoutMs;
  } else {
    deadline_reached = (now_ms >= end_time_ms);
    if (deadline_reached) {
      // If deadline is already reached if we keep going we will want to use
      // long sleeps since we expect to be woken up based on conditions.
      sleep_ms = kTestTimeoutMs;
    } else {
      sleep_ms = end_time_ms - now_ms;
    }
  }

  // Note that we may end up going past the deadline in order to make sure
  // that at least the metadata cache lookups have a chance to come in.
  if (!IsDone(wait_mode, deadline_reached)) {
    scheduler_->TimedWait(
        sleep_ms,
        MakeFunction(this, &RewriteDriver::TryCheckForCompletion,
                     wait_mode, end_time_ms, done));
  } else {
    // Done.
    waiting_ = kNoWait;
    done->CallRun();
  }
}

bool RewriteDriver::IsDone(WaitMode wait_mode, bool deadline_reached) {
  // Before deadline, we're happy only if we're 100% done.
  if (!deadline_reached) {
    return RewritesComplete() &&
           !((wait_mode == kWaitForShutDown) && HaveBackgroundFetchRewrite());
  } else {
    // When we've reached the deadline, if we're Render()'ing
    // we also give the jobs we can serve from cache a chance to finish
    // (so they always render).
    // We do not have to worry about possibly_quick_rewrites_ not being
    // incremented yet as jobs are only initiated from the HTML parse thread.
    if (wait_mode == kWaitForCachedRender) {
      return (possibly_quick_rewrites_ == 0);
    } else {
      return true;
    }
  }
}

void RewriteDriver::ExecuteFlushIfRequested() {
  if (flush_requested_) {
    Flush();
  }
}

void RewriteDriver::ExecuteFlushIfRequestedAsync(Function* callback) {
  if (flush_requested_) {
    FlushAsync(callback);
  } else {
    callback->CallRun();
  }
}

// This function should only be called at the beginning of a flush.  It moves
// the first inhibited event on queue_, and everything that follows it, onto
// deferred_queue_.  These events will be moved back onto queue_ before the
// flush is complete.
void RewriteDriver::SplitQueueIfNecessary() {
  ScopedMutex lock(inhibits_mutex_.get());
  if (end_elements_inhibited_.empty()) {
    return;
  }

  ConstHtmlEventSet inhibited_events;
  // The end() for an element may become available at any time, so we have to
  // rebuild the list of inhibited events on each call.
  ConstHtmlElementSet::iterator it = end_elements_inhibited_.begin();
  for ( ; it != end_elements_inhibited_.end(); ++it) {
    HtmlEvent* event = GetEndElementEvent(*it);
    if (event != NULL) {
      inhibited_events.insert(event);
    }
  }
  DCHECK(deferred_queue_.empty());
  inhibiting_event_ = SplitQueueOnFirstEventInSet(inhibited_events,
                                                  &deferred_queue_);
}

void RewriteDriver::Flush() {
  SchedulerBlockingFunction wait(scheduler_);
  FlushAsync(&wait);
  wait.Block();
  flush_requested_ = false;
}

void RewriteDriver::FlushAsync(Function* callback) {
  {
    ScopedMutex lock(inhibits_mutex_.get());
    DCHECK(!flush_in_progress_);
    flush_in_progress_ = true;
  }
  flush_requested_ = false;
  // Hide the tail of the queue after an inhibited event.
  SplitQueueIfNecessary();

  for (int i = 0, n = pre_render_filters_.size(); i < n; ++i) {
    HtmlFilter* filter = pre_render_filters_[i];
    ApplyFilter(filter);
  }

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
    // runs in the Rewrite thread.  Note that the DCHECK above, of
    // initiated_rewrites_.empty(), is a READ and it's OK to have
    // concurrent READs.
    ScopedMutex lock(rewrite_mutex());
    initiated_rewrites_.insert(rewrites_.begin(), rewrites_.end());

    // We must also start tasks while holding the lock, as otherwise a
    // successor task may complete and delete itself before we see if we
    // are the ones to start it.
    for (int i = 0; i < num_rewrites; ++i) {
      RewriteContext* rewrite_context = rewrites_[i];
      if (!rewrite_context->chained()) {
        rewrite_context->Initiate();
      }
    }
  }
  rewrites_.clear();

  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK(!fetch_queued_);
    Function* flush_async_done =
        MakeFunction(this, &RewriteDriver::QueueFlushAsyncDone,
                     num_rewrites, callback);
    if (resource_manager_->block_until_completion_in_render()) {
      CheckForCompletionAsync(kWaitForCompletion, -1, flush_async_done);
    } else {
      CheckForCompletionAsync(kWaitForCachedRender, rewrite_deadline_ms_,
                              flush_async_done);
    }
  }
}

void RewriteDriver::QueueFlushAsyncDone(int num_rewrites, Function* callback) {
  html_worker_->Add(MakeFunction(this, &RewriteDriver::FlushAsyncDone,
                                 num_rewrites, callback));
}

void RewriteDriver::FlushAsyncDone(int num_rewrites, Function* callback) {
  ScopedMutex lock(rewrite_mutex());
  DCHECK_EQ(0, possibly_quick_rewrites_);
  int completed_rewrites = num_rewrites - pending_rewrites_;

  // If the output cache lookup came as a HIT in after the deadline, that
  // means that (a) we can't use the result and (b) we don't need
  // to re-initiate the rewrite since it was in fact in cache.  Hopefully
  // the cache system will respond to HIT by making the next HIT faster
  // so it meets our deadline.  In either case we will track with stats.
  //
  RewriteStats* stats = resource_manager_->rewrite_stats();
  stats->cached_output_hits()->Add(completed_rewrites);
  stats->cached_output_missed_deadline()->Add(pending_rewrites_);

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

  HtmlParse::Flush();  // Clears the queue_.
  // Restore the tail of the queue_: an inhibited event and subsequent events.
  AppendEventsToQueue(&deferred_queue_);
  {
    ScopedMutex lock(inhibits_mutex_.get());
    DCHECK(flush_in_progress_);
    flush_in_progress_ = false;
    inhibiting_event_ = NULL;
    if (uninhibit_reflush_requested_) {
      // The flush that is currently concluding uninhibited an element.
      // We therefore need to flush again, and eat the callback until that
      // flush is complete.
      uninhibit_reflush_requested_ = false;
      html_worker_->Add(
                MakeFunction(this,
                             &RewriteDriver::FlushAsync,
                             MakeFunction(this,
                                          &RewriteDriver::UninhibitFlushDone,
                                          callback)));
      return;
    }
  }
  callback->CallRun();
}

const char* RewriteDriver::kPassThroughRequestAttributes[3] = {
  HttpAttributes::kIfModifiedSince,
  HttpAttributes::kReferer,
  HttpAttributes::kUserAgent
};

void RewriteDriver::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
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
    MetaTagFilter::Initialize(statistics);
    UrlLeftTrimFilter::Initialize(statistics);
  }
  CssFilter::Initialize(statistics);
  CssInlineImportToLinkFilter::Initialize(statistics);
}

void RewriteDriver::SetResourceManager(ResourceManager* resource_manager) {
  DCHECK(resource_manager_ == NULL);
  resource_manager_ = resource_manager;
  scheduler_ = resource_manager_->scheduler();
  set_timer(resource_manager->timer());
  inhibits_mutex_.reset(resource_manager_->thread_system()->NewMutex());
  rewrite_worker_ = resource_manager_->rewrite_workers()->NewSequence();
  html_worker_ = resource_manager_->html_workers()->NewSequence();
  low_priority_rewrite_worker_ =
      resource_manager_->low_priority_rewrite_workers()->NewSequence();
  scheduler_->RegisterWorker(rewrite_worker_);
  scheduler_->RegisterWorker(html_worker_);
  scheduler_->RegisterWorker(low_priority_rewrite_worker_);

  DCHECK(resource_filter_map_.empty());

  // Add the rewriting filters to the map unconditionally -- we may
  // need the to process resource requests due to a query-specific
  // 'rewriters' specification.  We still use the passed-in options
  // to determine whether they get added to the html parse filter chain.
  // Note: RegisterRewriteFilter takes ownership of these filters.
  CacheExtender* cache_extender = new CacheExtender(this);
  ImageCombineFilter* image_combiner = new ImageCombineFilter(this);
  ImageRewriteFilter* image_rewriter = new ImageRewriteFilter(this);

  RegisterRewriteFilter(new CssCombineFilter(this));
  RegisterRewriteFilter(
      new CssFilter(this, cache_extender, image_rewriter, image_combiner));
  RegisterRewriteFilter(new JavascriptFilter(this));
  RegisterRewriteFilter(new JsCombineFilter(this));
  RegisterRewriteFilter(image_rewriter);
  RegisterRewriteFilter(cache_extender);
  RegisterRewriteFilter(image_combiner);

  // These filters are needed to rewrite and trim urls in modified CSS files.
  domain_rewriter_.reset(new DomainRewriteFilter(this, statistics()));
  url_trim_filter_.reset(new UrlLeftTrimFilter(this, statistics()));
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

bool RewriteDriver::UserAgentSupportsImageInlining() const {
  if (user_agent_supports_image_inlining_ == kNotSet) {
    user_agent_supports_image_inlining_ =
        user_agent_matcher_.SupportsImageInlining(user_agent_) ?
        kTrue : kFalse;
  }
  return (user_agent_supports_image_inlining_ == kTrue);
}
bool RewriteDriver::UserAgentSupportsWebp() const {
  if (user_agent_supports_webp_ == kNotSet) {
    user_agent_supports_webp_ =
        user_agent_matcher_.SupportsWebp(user_agent_) ? kTrue : kFalse;
  }
  return (user_agent_supports_webp_ == kTrue);
}

void RewriteDriver::AddFilters() {
  CHECK(html_writer_filter_ == NULL);
  CHECK(!filters_added_);
  if (has_custom_options()) {
    resource_manager_->ComputeSignature(custom_options_.get());
  }
  filters_added_ = true;

  AddPreRenderFilters();
  AddPostRenderFilters();
}

void RewriteDriver::AddPreRenderFilters() {
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

  if (rewrite_options->flush_html()) {
    // Note that this does not get hooked into the normal html-parse
    // filter-chain as it gets run immediately after every call to
    // ParseText, possibly inducing the system to trigger a Flush
    // based on the content it sees.
    set_event_listener(new FlushHtmlFilter(this));
  }

  if (rewrite_options->Enabled(RewriteOptions::kAddHead) ||
      rewrite_options->Enabled(RewriteOptions::kCombineHeads) ||
      rewrite_options->Enabled(RewriteOptions::kMoveCssToHead) ||
      rewrite_options->Enabled(RewriteOptions::kMakeGoogleAnalyticsAsync) ||
      rewrite_options->Enabled(RewriteOptions::kAddInstrumentation)) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddOwnedPreRenderFilter(new AddHeadFilter(
        this, rewrite_options->Enabled(RewriteOptions::kCombineHeads)));
  }
  if (rewrite_options->Enabled(RewriteOptions::kStripScripts)) {
    // Experimental filter that blindly strips all scripts from a page.
    AddOwnedPreRenderFilter(new StripScriptsFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineImportToLink)) {
    // If we're converting simple embedded CSS @imports into a href link
    // then we need to do that before any other CSS processing.
    AddOwnedPreRenderFilter(new CssInlineImportToLinkFilter(this,
                                                            statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kOutlineCss)) {
    // Cut out inlined styles and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    CssOutlineFilter* css_outline_filter = new CssOutlineFilter(this);
    AddOwnedPreRenderFilter(css_outline_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kOutlineJavascript)) {
    // Cut out inlined scripts and make them into external resources.
    // This can only be called once and requires a resource_manager to be set.
    CHECK(resource_manager_ != NULL);
    JsOutlineFilter* js_outline_filter = new JsOutlineFilter(this);
    AddOwnedPreRenderFilter(js_outline_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kMoveCssToHead)) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AddOwnedPreRenderFilter(new CssMoveToHeadFilter(this, statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineCss)) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a resource_manager to be set.
    EnableRewriteFilter(RewriteOptions::kCssCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteCss)) {
    EnableRewriteFilter(RewriteOptions::kCssFilterId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kMakeGoogleAnalyticsAsync)) {
    // Converts sync loads of Google Analytics javascript to async loads.
    // This needs to be listed before rewrite_javascript because it injects
    // javascript that has comments and extra whitespace.
    AddOwnedPreRenderFilter(new GoogleAnalyticsFilter(this, statistics()));
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteJavascript)) {
    // Rewrite (minify etc.) JavaScript code to reduce time to first
    // interaction.
    EnableRewriteFilter(RewriteOptions::kJavascriptMinId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineJavascript)) {
    // Combine external JS resources. Done after minification and analytics
    // detection, as it converts script sources into string literals, making
    // them opaque to analysis.
    EnableRewriteFilter(RewriteOptions::kJavascriptCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineCss)) {
    // Inline small CSS files.  Give CssCombineFilter and CSS minification a
    // chance to run before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddOwnedPreRenderFilter(new CssInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineJavascript)) {
    // Inline small Javascript files.  Give JS minification a chance to run
    // before we decide what counts as "small".
    CHECK(resource_manager_ != NULL);
    AddOwnedPreRenderFilter(new JsInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kConvertJpegToWebp) ||
      rewrite_options->Enabled(RewriteOptions::kConvertJpegToProgressive) ||
      rewrite_options->NeedLowResImages() ||
      rewrite_options->Enabled(RewriteOptions::kInlineImages) ||
      rewrite_options->Enabled(RewriteOptions::kInsertImageDimensions) ||
      rewrite_options->Enabled(RewriteOptions::kRecompressImages) ||
      rewrite_options->Enabled(RewriteOptions::kResizeImages)) {
    EnableRewriteFilter(RewriteOptions::kImageCompressionId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRemoveComments)) {
    AddOwnedPreRenderFilter(new RemoveCommentsFilter(this, rewrite_options));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCollapseWhitespace)) {
    // Remove excess whitespace in HTML
    AddOwnedPreRenderFilter(new CollapseWhitespaceFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kElideAttributes)) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AddOwnedPreRenderFilter(new ElideAttributesFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kExtendCacheCss) ||
      rewrite_options->Enabled(RewriteOptions::kExtendCacheImages) ||
      rewrite_options->Enabled(RewriteOptions::kExtendCacheScripts)) {
    // Extend the cache lifetime of resources.
    EnableRewriteFilter(RewriteOptions::kCacheExtenderId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kSpriteImages)) {
    EnableRewriteFilter(RewriteOptions::kImageCombineId);
  }
}

void RewriteDriver::AddPostRenderFilters() {
  const RewriteOptions* rewrite_options = options();
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
    AddUnownedPostRenderFilter(domain_rewriter_.get());
  }
  if (rewrite_options->Enabled(RewriteOptions::kDivStructure)) {
    // Adds a query parameter to each link roughly designating its position on
    // the page to be used in target, referer counting, which is then to be
    // used to augment prefetch/prerender optimizations.  Should happen before
    // RemoveQuotes.
    AddOwnedPostRenderFilter(new DivStructureFilter());
  }
  if (rewrite_options->Enabled(RewriteOptions::kLeftTrimUrls)) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    AddUnownedPostRenderFilter(url_trim_filter_.get());
  }
  if (rewrite_options->Enabled(RewriteOptions::kDeferJavascript)) {
    // Defers javascript download and execution to post onload.
    AddUnownedPostRenderFilter(new JsDisableFilter(this));
    AddUnownedPostRenderFilter(new JsDeferDisabledFilter(this));
  }
  // TODO(nikhilmadan): Should we disable this for bots?
  if (rewrite_options->Enabled(RewriteOptions::kLazyloadImages)) {
    AddOwnedPostRenderFilter(new LazyloadImagesFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kRemoveQuotes)) {
    // Remove extraneous quotes from html attributes.  Does this save
    // enough bytes to be worth it after compression?  If we do it
    // everywhere it seems to give a small savings.
    AddOwnedPostRenderFilter(new HtmlAttributeQuoteRemoval(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kAddInstrumentation)) {
    // Inject javascript to instrument loading-time.
    add_instrumentation_filter_ = new AddInstrumentationFilter(
        this, rewrite_options->beacon_url());
    AddOwnedPostRenderFilter(add_instrumentation_filter_);
  }
  if (rewrite_options->Enabled(RewriteOptions::kConvertMetaTags)) {
    AddOwnedPostRenderFilter(new MetaTagFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDisableJavascript)) {
    AddOwnedPostRenderFilter(new JsDisableFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDelayImages)) {
    // kInsertImageDimensions should be enabled to avoid drastic reflows.
    AddOwnedPostRenderFilter(new DelayImagesFilter(this));
  }
  // NOTE(abliss): Adding a new filter?  Does it export any statistics?  If it
  // doesn't, it probably should.  If it does, be sure to add it to the
  // Initialize() function above or it will break under Apache!
}

void RewriteDriver::AddOwnedPreRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::AddOwnedPostRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  AddUnownedPostRenderFilter(filter);
}

void RewriteDriver::AddUnownedPostRenderFilter(HtmlFilter* filter) {
  HtmlParse::AddFilter(filter);
}

// This is used exclusively in tests.
void RewriteDriver::AddRewriteFilter(RewriteFilter* filter) {
  RegisterRewriteFilter(filter);
  EnableRewriteFilter(filter->id());
}

void RewriteDriver::EnableRewriteFilter(const char* id) {
  RewriteFilter* filter = resource_filter_map_[id];
  CHECK(filter);
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::RegisterRewriteFilter(RewriteFilter* filter) {
  // Track resource_fetches if we care about statistics.  Note that
  // the statistics are owned by the resource manager, which generally
  // should be set up prior to the rewrite_driver.
  //
  // TODO(sligocki): It'd be nice to get this into the constructor.
  resource_filter_map_[filter->id()] = filter;
  filters_to_delete_.push_back(filter);
}

void RewriteDriver::SetWriter(Writer* writer) {
  writer_ = writer;
  if (html_writer_filter_ == NULL) {
    html_writer_filter_.reset(new HtmlWriterFilter(this));
    html_writer_filter_->set_case_fold(options()->lowercase_html_names());
    if (options()->Enabled(RewriteOptions::kHtmlWriterFilter)) {
      HtmlParse::AddFilter(html_writer_filter_.get());
    }
  }

  html_writer_filter_->set_writer(writer);
}

Statistics* RewriteDriver::statistics() const {
  return (resource_manager_ == NULL) ? NULL : resource_manager_->statistics();
}

bool RewriteDriver::DecodeOutputResourceName(const GoogleUrl& gurl,
                                             ResourceNamer* namer_out,
                                             OutputResourceKind* kind_out,
                                             RewriteFilter** filter_out) const {
  // First, we can't handle anything that's not a valid URL nor is named
  // properly as our resource.
  if (!gurl.is_valid()) {
    return false;
  }

  StringPiece name = gurl.LeafSansQuery();
  if (!namer_out->Decode(name)) {
    return false;
  }

  // URLs without any hash are rejected as well, as they do not produce
  // OutputResources with a computable URL. (We do accept 'wrong' hashes since
  // they could come up legitimately under some asynchrony scenarios)
  if (namer_out->hash().empty()) {
    return false;
  }

  // If we are running in proxy mode we need to ignore URLs where the leaf is
  // encoded but the URL as a whole isn't proxy encoded, since that can happen
  // when proxying from a server using mod_pagsepeed.
  UrlNamer* url_namer = resource_manager()->url_namer();
  if (url_namer->ProxyMode() && !url_namer->IsProxyEncoded(gurl)) {
    return false;
  }

  // Now let's reject as mal-formed if the id string is not
  // in the rewrite drivers. Also figure out the filter's preferred
  // resource kind.
  StringPiece id = namer_out->id();
  *kind_out = kRewrittenResource;
  StringFilterMap::const_iterator p = resource_filter_map_.find(
      GoogleString(id.data(), id.size()));
  if (p != resource_filter_map_.end()) {
    *filter_out = p->second;
    if ((*filter_out)->ComputeOnTheFly()) {
      *kind_out = kOnTheFlyResource;
    }
  } else if ((id == CssOutlineFilter::kFilterId) ||
             (id == JsOutlineFilter::kFilterId)) {
    // OutlineFilter is special because it's not a RewriteFilter -- it's
    // just an HtmlFilter, but it does encode rewritten resources that
    // must be served from the cache.
    //
    // TODO(jmarantz): figure out a better way to refactor this.
    // TODO(jmarantz): add a unit-test to show serving outline-filter resources.
    *kind_out = kOutlinedResource;
    *filter_out = NULL;
  } else {
    return false;
  }

  return true;
}

OutputResourcePtr RewriteDriver::DecodeOutputResource(
    const GoogleUrl& gurl,
    RewriteFilter** filter) const {
  ResourceNamer namer;
  OutputResourceKind kind;
  if (!DecodeOutputResourceName(gurl, &namer, &kind, filter)) {
    return OutputResourcePtr();
  }

  StringPiece base = gurl.AllExceptLeaf();
  OutputResourcePtr output_resource(new OutputResource(
      resource_manager_, base, base, base, namer,
      NULL,  // content_type
      options(), kind));

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

class FilterFetch : public SharedAsyncFetch {
 public:
  FilterFetch(RewriteDriver* driver, AsyncFetch* async_fetch)
      : SharedAsyncFetch(async_fetch),
        driver_(driver) {
  }
  virtual ~FilterFetch() {}

  static bool Start(RewriteFilter* filter,
                    const OutputResourcePtr& output_resource,
                    AsyncFetch* async_fetch,
                    MessageHandler* handler) {
    RewriteDriver* driver = filter->driver();
    FilterFetch* filter_fetch = new FilterFetch(driver, async_fetch);

    bool queued = false;
    RewriteContext* context = filter->MakeRewriteContext();
    DCHECK(context != NULL);
    if (context != NULL) {
      queued = context->Fetch(output_resource, filter_fetch, handler);
    }
    if (!queued) {
      RewriteStats* stats = driver->resource_manager()->rewrite_stats();
      stats->failed_filter_resource_fetches()->Add(1);
      async_fetch->Done(false);
      driver->FetchComplete();
      delete filter_fetch;
    }
    return queued;
  }

 protected:
  virtual void HandleDone(bool success) {
    RewriteStats* stats = driver_->resource_manager()->rewrite_stats();
    if (success) {
      stats->succeeded_filter_resource_fetches()->Add(1);
    } else {
      stats->failed_filter_resource_fetches()->Add(1);
    }
    base_fetch()->Done(success);
    driver_->FetchComplete();
    delete this;
  }

 private:
  RewriteDriver* driver_;
};

class CacheCallback : public OptionsAwareHTTPCacheCallback {
 public:
  CacheCallback(RewriteDriver* driver,
                RewriteFilter* filter,
                const OutputResourcePtr& output_resource,
                AsyncFetch* async_fetch,
                MessageHandler* handler)
      : OptionsAwareHTTPCacheCallback(driver->options()),
        driver_(driver),
        filter_(filter),
        output_resource_(output_resource),
        async_fetch_(async_fetch),
        handler_(handler),
        did_locking_(false) {
  }

  virtual ~CacheCallback() {}

  void Find() {
    ResourceManager* resource_manager = driver_->resource_manager();
    HTTPCache* http_cache = resource_manager->http_cache();
    http_cache->Find(output_resource_->url(), handler_, this);
  }

  virtual void Done(HTTPCache::FindResult find_result) {
    StringPiece content;
    ResponseHeaders* response_headers = async_fetch_->response_headers();
    if (find_result == HTTPCache::kFound) {
      HTTPValue* value = http_value();
      bool success = (value->ExtractContents(&content) &&
                      value->ExtractHeaders(response_headers, handler_));
      if (success) {
        output_resource_->Link(value, handler_);
        output_resource_->set_written(true);
        success = async_fetch_->Write(content, handler_);
      }
      async_fetch_->Done(success);
      driver_->FetchComplete();
      delete this;
    } else if (did_locking_) {
      if (output_resource_->Load(handler_)) {
        // OutputResources can also be loaded while not in cache if
        // store_outputs_in_file_system() is true.
        content = output_resource_->contents();
        response_headers->CopyFrom(*output_resource_->response_headers());
        ResourceManager* resource_manager = driver_->resource_manager();
        HTTPCache* http_cache = resource_manager->http_cache();
        http_cache->Put(output_resource_->url(), response_headers,
                        content, handler_);
        async_fetch_->Done(async_fetch_->Write(content, handler_));
        driver_->FetchComplete();
      } else {
        // We already had the lock and failed our cache lookup.  Use the filter
        // to reconstruct.
        if (filter_ != NULL) {
          FilterFetch::Start(filter_, output_resource_, async_fetch_, handler_);
        } else {
          response_headers->SetStatusAndReason(HttpStatus::kNotFound);
          async_fetch_->Done(false);
          driver_->FetchComplete();
        }
      }
      delete this;
    } else {
      // Take creation lock and re-try operation (did_locking_ will hold and we
      // won't get here again). Note that we purposefully continue here even if
      // locking fails (so that stale locks not old enough to steal wouldn't
      // cause us to needlessly fail fetches); which is also why we use
      // did_locking_ above and not has_lock().
      did_locking_ = true;
      // The use of rewrite_worker() here is for more predictability in
      // testing, as it keeps the individual lock ops ordered with respect
      // to the rewrite graph state machine.
      output_resource_->LockForCreation(
          driver_->rewrite_worker(),
          MakeFunction(this, &CacheCallback::Find, &CacheCallback::Find));
    }
  }

 private:
  RewriteDriver* driver_;
  RewriteFilter* filter_;
  OutputResourcePtr output_resource_;
  AsyncFetch* async_fetch_;
  MessageHandler* handler_;
  bool did_locking_;
};

}  // namespace

bool RewriteDriver::FetchResource(const StringPiece& url,
                                  AsyncFetch* async_fetch) {
  DCHECK(!fetch_queued_) << this;
  DCHECK_EQ(0, pending_rewrites_) << this;
  bool handled = false;

  // Note that this does permission checking and parsing of the url, but doesn't
  // actually fetch any data until we specifically ask it to.
  RewriteFilter* filter = NULL;
  GoogleUrl gurl(url);
  OutputResourcePtr output_resource(DecodeOutputResource(gurl, &filter));

  if (output_resource.get() != NULL) {
    handled = true;
    FetchOutputResource(output_resource, filter, async_fetch);
  } else if (options()->ajax_rewriting_enabled()) {
    // This is an ajax resource.
    handled = true;
    StringPiece base = gurl.AllExceptLeaf();
    ResourceNamer namer;
    output_resource.reset(new OutputResource(resource_manager_, base, base,
        base, namer, NULL, options(), kRewrittenResource));
    SetBaseUrlForFetch(url);
    fetch_queued_ = true;
    AjaxRewriteContext* context = new AjaxRewriteContext(this, url.data());
    context->Fetch(output_resource, async_fetch, message_handler());
  }
  return handled;
}

bool RewriteDriver::FetchOutputResource(
    const OutputResourcePtr& output_resource,
    RewriteFilter* filter,
    AsyncFetch* async_fetch) {
  // None of our resources ever change -- the hash of the content is embedded
  // in the filename.  This is why we serve them with very long cache
  // lifetimes.  However, when the user presses Reload, the browser may
  // attempt to validate that the cached copy is still fresh by sending a GET
  // with an If-Modified-Since header.  If this header is present, we should
  // return a 304 Not Modified, since any representation of the resource
  // that's in the browser's cache must be correct.
  bool queued = false;
  ConstStringStarVector values;
  if (async_fetch->request_headers()->Lookup(HttpAttributes::kIfModifiedSince,
                                             &values)) {
    async_fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kNotModified);
    async_fetch->HeadersComplete();
    async_fetch->Done(true);
    queued = false;
  } else {
    SetBaseUrlForFetch(output_resource->url());
    fetch_queued_ = true;
    if (output_resource->kind() == kOnTheFlyResource) {
      // Don't bother to look up the resource in the cache: ask the filter.
      if (filter != NULL) {
        queued = FilterFetch::Start(filter, output_resource, async_fetch,
                                    message_handler());
      }
    } else {
      CacheCallback* cache_callback = new CacheCallback(
          this, filter, output_resource, async_fetch, message_handler());
      cache_callback->Find();
      queued = true;
    }
  }
  return queued;
}

void RewriteDriver::FetchComplete() {
  ScopedMutex lock(rewrite_mutex());
  if (!fetch_detached_) {
    FetchCompleteImpl(true /* want to signal*/, &lock);
  } else {
    DCHECK(!detached_fetch_main_path_complete_);
    detached_fetch_main_path_complete_ = true;
    if (detached_fetch_detached_path_complete_) {
      FetchCompleteImpl(true /* want to signal*/, &lock);
    } else {
      // Make sure to mark us as having no active fetch for
      // purposes of RewritesComplete()
      fetch_queued_ = false;
      scheduler_->Signal();
    }
  }
}

void RewriteDriver::DetachFetch() {
  ScopedMutex lock(rewrite_mutex());
  fetch_detached_ = true;
}

void RewriteDriver::DetachedFetchComplete() {
  ScopedMutex lock(rewrite_mutex());

  DCHECK(fetch_detached_);
  DCHECK(!detached_fetch_detached_path_complete_);
  detached_fetch_detached_path_complete_ = true;
  if (detached_fetch_main_path_complete_) {
    FetchCompleteImpl(false, /* do not signal, was done on FetchComplete*/
                      &lock);
  }
}

void RewriteDriver::FetchCompleteImpl(bool signal, ScopedMutex* lock) {
  DCHECK_EQ(fetch_queued_, signal);
  DCHECK_EQ(0, pending_rewrites_);

  fetch_queued_ = false;
  STLDeleteElements(&rewrites_);
  if (signal) {
    scheduler_->Signal();
  }
  lock->Release();

  if (cleanup_on_fetch_complete_) {
    // If cleanup_on_fetch_complete_ is set, the main thread has already tried
    // to call Cleanup on us, so it's not going to be touching us any more ---
    // and so this is race-free.
    Cleanup();
  }
}

bool RewriteDriver::MayRewriteUrl(const GoogleUrl& domain_url,
                                  const GoogleUrl& input_url) const {
  bool ret = false;
  if (domain_url.is_valid()) {
    if (options()->IsAllowed(input_url.Spec())) {
      ret = options()->domain_lawyer()->IsDomainAuthorized(
          domain_url, input_url);
    }
  }
  return ret;
}

ResourcePtr RewriteDriver::CreateInputResource(const GoogleUrl& input_url) {
  ResourcePtr resource;
  bool may_rewrite = false;
  if (decoded_base_url_.is_valid()) {
    may_rewrite = MayRewriteUrl(decoded_base_url_, input_url);
  } else {
    // Shouldn't happen?
    message_handler()->Message(
        kFatal, "invalid decoded_base_url_ for '%s'", input_url.spec_c_str());
    DLOG(FATAL);
  }
  if (may_rewrite) {
    resource = CreateInputResourceUnchecked(input_url);
  } else if (input_url.SchemeIs("data")) {
    // skip and silently ignore; don't log a failure.
  } else {
    message_handler()->Message(kInfo, "No permission to rewrite '%s'",
                               input_url.spec_c_str());
    RewriteStats* stats = resource_manager_->rewrite_stats();
    stats->resource_url_domain_rejections()->Add(1);
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
                                 url.spec_c_str());
    }
  } else if (url.SchemeIs("http") || url.SchemeIs("https")) {
    // Note: type may be NULL if url has an unexpected or malformed extension.
    const ContentType* type = NameExtensionToContentType(url_string);
    GoogleString filename;
    if (options()->file_load_policy()->ShouldLoadFromFile(url, &filename)) {
      resource.reset(new FileInputResource(resource_manager_, options(), type,
                                           url_string, filename));
    } else {
      // If the scheme is https and the fetcher doesn't support https, map
      // the URL to what will ultimately be fetched to see if that will be
      // http, in which case the fetcher will be able to handle it.
      // TODO(matterbury): If/when we support origin mapping TO https this
      // test will need fixing to always map the origin.
      if (url.SchemeIs("https") && !url_async_fetcher_->SupportsHttps()) {
        GoogleString mapped_url;
        options()->domain_lawyer()->MapOriginUrl(url, &mapped_url);
        GoogleUrl mapped_gurl(mapped_url);
        if (!mapped_gurl.SchemeIs("http")) {
          message_handler()->Message(
              kInfo, "Cannot fetch url '%s': as https is not supported",
              url.spec_c_str());
          return resource;
        }
      }
      resource.reset(new UrlInputResource(resource_manager_, options(), type,
                                          url_string));
    }
  } else {
    // Note: Valid user-content can leave us here.
    // Specifically, any URLs with scheme other than data: or http: or https:.
    // TODO(sligocki): Is this true? Or will such URLs not make it this far?
    message_handler()->Message(kWarning, "Unsupported scheme '%s' for url '%s'",
                               url.Scheme().as_string().c_str(),
                               url.spec_c_str());
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
  {
    ScopedMutex lock(rewrite_mutex());
    parsing_ = true;
  }

  if (ret) {
    base_was_set_ = false;
    if (is_url_valid()) {
      base_url_.Reset(google_url());
      SetDecodedUrlFromBase();
    }
  }
  return ret;
}

void RewriteDriver::SetDecodedUrlFromBase() {
  UrlNamer* namer = resource_manager()->url_namer();
  GoogleString decoded_base;
  if (namer->Decode(base_url_, NULL, &decoded_base)) {
    decoded_base_url_.Reset(decoded_base);
  } else {
    decoded_base_url_.Reset(base_url_);
  }
  DCHECK(decoded_base_url_.is_valid());
}

void RewriteDriver::RewriteComplete(RewriteContext* rewrite_context) {
  ScopedMutex lock(rewrite_mutex());
  DCHECK(!fetch_queued_);
  bool signal = false;
  bool attached = false;
  RewriteContextSet::iterator p = initiated_rewrites_.find(rewrite_context);
  if (p != initiated_rewrites_.end()) {
    initiated_rewrites_.erase(p);
    attached = true;

    --pending_rewrites_;
    if (!rewrite_context->slow()) {
      --possibly_quick_rewrites_;
      if ((possibly_quick_rewrites_ == 0) &&
          (waiting_ == kWaitForCachedRender)) {
        signal = true;
      }
    }

    if (pending_rewrites_ == 0) {
      signal = true;
    }
  } else {
    int erased = detached_rewrites_.erase(rewrite_context);
    CHECK_EQ(1, erased) << " rewrite_context " << rewrite_context
                        << " not in either detached_rewrites or "
                        << "initiated_rewrites_";
    if ((waiting_ == kWaitForCompletion || waiting_ == kWaitForShutDown) &&
        detached_rewrites_.empty()) {
      signal = true;
    }
  }
  rewrite_context->Propagate(attached);
  ++rewrites_to_delete_;
  if (signal) {
    DCHECK(!fetch_queued_);
    scheduler_->Signal();
  }
}

void RewriteDriver::ReportSlowRewrites(int num) {
  ScopedMutex lock(rewrite_mutex());
  possibly_quick_rewrites_ -= num;
  CHECK_LE(0, possibly_quick_rewrites_) << base_url_.Spec();
  if ((possibly_quick_rewrites_ == 0) && (waiting_ == kWaitForCachedRender)) {
    scheduler_->Signal();
  }
}

void RewriteDriver::DeleteRewriteContext(RewriteContext* rewrite_context) {
  bool ready_to_recycle = false;
  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_LT(0, rewrites_to_delete_);
    --rewrites_to_delete_;
    delete rewrite_context;
    if (RewritesComplete()) {
      if (waiting_ != kNoWait) {
        scheduler_->Signal();
      } else {
        ready_to_recycle = !externally_managed_ && !parsing_;
      }
    }
  }
  if (ready_to_recycle) {
    resource_manager_->ReleaseRewriteDriver(this);
  }
}

RewriteContext* RewriteDriver::RegisterForPartitionKey(
    const GoogleString& partition_key, RewriteContext* candidate) {
  std::pair<PrimaryRewriteContextMap::iterator, bool> insert_result =
      primary_rewrite_context_map_.insert(
          std::make_pair(partition_key, candidate));
  if (insert_result.second) {
    // Our value is new, so just return NULL.
    return NULL;
  } else {
    // Insert failed, return the old value.
    return insert_result.first->second;
  }
}

void RewriteDriver::DeregisterForPartitionKey(const GoogleString& partition_key,
                                              RewriteContext* rewrite_context) {
  // If the context being deleted is the primary for some cache key,
  // deregister it.
  PrimaryRewriteContextMap::iterator i =
      primary_rewrite_context_map_.find(partition_key);
  if ((i != primary_rewrite_context_map_.end()) &&
      (i->second == rewrite_context)) {
    primary_rewrite_context_map_.erase(i);
  }
}

void RewriteDriver::Cleanup() {
  if (!externally_managed_) {
    bool done = false;
    {
      ScopedMutex lock(rewrite_mutex());
      done = RewritesComplete();
      if (!done) {
        parsing_ = false;  // Permit recycle when contexts done.
        if (fetch_queued_) {
          // Asynchronous resource fetch we gave up on --- make sure to cleanup
          // ourselves when we are done.
          cleanup_on_fetch_complete_ = true;
        }
      } else {
        // Even if we're finished, we may still have a fetch job trying to do
        // some work in the background.
        if (HaveBackgroundFetchRewrite()) {
          cleanup_on_fetch_complete_ = true;
          done = false;
        }
      }
    }
    if (done) {
      resource_manager_->ReleaseRewriteDriver(this);
    }
  }
}

void RewriteDriver::InhibitEndElement(const HtmlElement* element) {
  // Since element->end() may not exist yet, we must store the actual element
  // pointer.
  ScopedMutex lock(inhibits_mutex_.get());
  if (element == NULL) {
    return;
  }
  end_elements_inhibited_.insert(element);
}

int RewriteDriver::UninhibitEndElementFlushless(const HtmlElement* element) {
  ScopedMutex lock(inhibits_mutex_.get());
  return end_elements_inhibited_.erase(element);
}

// Uninhibit the EndElementEvent for element.
// This function may be called from another thread, typically a fetch callback.
void RewriteDriver::UninhibitEndElement(const HtmlElement* element) {
  if (UninhibitEndElementFlushless(element) == 1) {
    // The element was actually inhibited.  If it was at the front of the queue,
    // it was preventing everything that follows it on the queue from flushing.
    // Now that the inhibition is lifted, all that stuff needs to flush.

    // Since inhibits are used to make time for the filters to wait for a slow
    // remote input that affects the DOM, element almost certainly *was* at
    // front of the queue.  Rather than synchronize with queue_ to check, we
    // just flush.  This might occasionally be superfluous, but no harm is done.
    ScopedMutex lock(inhibits_mutex_.get());
    if (flush_in_progress_) {
      // This flag will cause FlushAsyncDone to eat the user callback and
      // schedule another flush.
      uninhibit_reflush_requested_ = true;
    } else if (finish_parse_on_hold_ != NULL) {
      // Schedule a flush.  If we aren't holding a FinishParse client callback,
      // it's not safe to schedule a flush because we might race with the
      // client.  That's OK, because the parse isn't finished; there will be
      // another flush eventually, and so we won't deadlock.

      html_worker_->Add(
          MakeFunction(this,
                       &RewriteDriver::FlushAsync,
                       MakeFunction(this,
                                    &RewriteDriver::UninhibitFlushDone,
                                    static_cast<Function *>(NULL))));
    }
  }
}

bool RewriteDriver::EndElementIsInhibited(const HtmlElement* element) {
  ScopedMutex lock(inhibits_mutex_.get());
  return end_elements_inhibited_.find(element) != end_elements_inhibited_.end();
}

bool RewriteDriver::EndElementIsStoppingFlush(const HtmlElement* element) {
  ScopedMutex lock(inhibits_mutex_.get());
  return (inhibiting_event_ != NULL &&
          inhibiting_event_->GetElementIfEndEvent() == element);
}

// Finish the parse if FinishParseAsync was previously held up by an inhibited
// event.  Otherwise, run the user callback.
void RewriteDriver::UninhibitFlushDone(Function* user_callback) {
  ScopedMutex lock(inhibits_mutex_.get());
  if (finish_parse_on_hold_ != NULL && end_elements_inhibited_.size() == 0) {
    html_worker_->Add(finish_parse_on_hold_);
    finish_parse_on_hold_ = NULL;
  }
  if (user_callback != NULL) {
    user_callback->CallRun();
  }
}

void RewriteDriver::FinishParse() {
  HtmlParse::FinishParse();
  Cleanup();
}

void RewriteDriver::FinishParseAsync(Function* callback) {
  HtmlParse::BeginFinishParse();
  FlushAsync(
      MakeFunction(this, &RewriteDriver::QueueFinishParseAfterFlush, callback));
}

void RewriteDriver::QueueFinishParseAfterFlush(Function* user_callback) {
  // Disconnected to relinquish the lock; might be a better idea to
  // instead have a lockless version of cleanup?
  html_worker_->Add(
      MakeFunction(this, &RewriteDriver::FinishParseAfterFlush, user_callback));
}

void RewriteDriver::FinishParseAfterFlush(Function* user_callback) {
  if (GetEventQueueSize() > 0) {
    // Something is blocking completion of the parse.  Save the callback.
    ScopedMutex lock(inhibits_mutex_.get());
    finish_parse_on_hold_ = MakeFunction(this,
                                         &RewriteDriver::FinishParseAfterFlush,
                                         user_callback);
    return;
  }
  HtmlParse::EndFinishParse();
  Cleanup();
  if (user_callback != NULL) {
    user_callback->CallRun();
  }
}

void RewriteDriver::InfoAt(RewriteContext* context, const char* msg, ...) {
  va_list args;
  va_start(args, msg);

  if ((context == NULL) || (context->num_slots() == 0)) {
    InfoHereV(msg, args);
  } else {
    GoogleString new_msg;
    for (int c = 0; c < context->num_slots(); ++c) {
      StrAppend(&new_msg, context->slot(c)->LocationString(),
                ((c == context->num_slots() - 1) ? ": " : " "));
    }
    StringAppendV(&new_msg, msg, args);
    message_handler()->Message(kInfo, "%s", new_msg.c_str());
  }

  va_end(args);
}

// Constructs an output resource corresponding to the specified input resource
// and encoded using the provided encoder.
OutputResourcePtr RewriteDriver::CreateOutputResourceFromResource(
    const StringPiece& filter_id,
    const UrlSegmentEncoder* encoder,
    const ResourceContext* data,
    const ResourcePtr& input_resource,
    OutputResourceKind kind,
    bool use_async_flow) {
  OutputResourcePtr result;
  if (input_resource.get() != NULL) {
    // TODO(jmarantz): It would be more efficient to pass in the base
    // document GURL or save that in the input resource.
    GoogleUrl gurl(input_resource->url());
    GoogleString mapped_domain;
    GoogleUrl mapped_gurl;
    // Get the domain and URL after any domain lawyer rewriting.
    if (options()->IsAllowed(gurl.Spec()) &&
        options()->domain_lawyer()->MapRequestToDomain(
            gurl, gurl.Spec(), &mapped_domain, &mapped_gurl,
            resource_manager_->message_handler())) {
      GoogleString name;
      StringVector v;
      v.push_back(mapped_gurl.LeafWithQuery().as_string());
      encoder->Encode(v, data, &name);
      result.reset(CreateOutputResourceWithMappedPath(
          mapped_gurl.AllExceptLeaf(), gurl.AllExceptLeaf(),
          filter_id, name, input_resource->type(), kind, use_async_flow));
    }
  }
  return result;
}

OutputResourcePtr RewriteDriver::CreateOutputResourceWithPath(
    const StringPiece& mapped_path,
    const StringPiece& unmapped_path,
    const StringPiece& base_url,
    const StringPiece& filter_id,
    const StringPiece& name,
    const ContentType* content_type,
    OutputResourceKind kind,
    bool use_async_flow /* ignored */) {
  ResourceNamer full_name;
  full_name.set_id(filter_id);
  full_name.set_name(name);
  if (content_type != NULL) {
    // TODO(jmaessen): The addition of 1 below avoids the leading ".";
    // make this convention consistent and fix all code.
    full_name.set_ext(content_type->file_extension() + 1);
  }
  OutputResourcePtr resource;

  int leaf_size = full_name.EventualSize(*resource_manager_->hasher());
  int url_size = mapped_path.size() + leaf_size;
  if ((leaf_size <= options()->max_url_segment_size()) &&
      (url_size <= options()->max_url_size())) {
    OutputResource* output_resource = new OutputResource(
        resource_manager_, mapped_path, unmapped_path, base_url,
        full_name, content_type, options(), kind);
    resource.reset(output_resource);
  }
  return resource;
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
      SetDecodedUrlFromBase();
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
  if (!decoded_base_url_.is_valid()) {
    // TODO(jmaessen): we're re-constructing a GoogleUrl after having already
    // done so (repeatedly over several calls) in DecodeOutputResource!  Gah!
    // We at least assume that base_url_ is valid since it was checked when
    // output_resource was created.
    base_url_.Reset(url);
    DCHECK(base_url_.is_valid());
    SetDecodedUrlFromBase();
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
  ++possibly_quick_rewrites_;
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

bool RewriteDriver::ShouldNotRewriteImages() const {
  if (user_agent_is_bot_ == kNotSet) {
    if (options()->botdetect_enabled() && BotChecker::Lookup(user_agent_)) {
      user_agent_is_bot_ = kTrue;
    } else {
      user_agent_is_bot_ = kFalse;
    }
  }
  return (user_agent_is_bot_ == kTrue);
}

bool RewriteDriver::MayCacheExtendCss() const {
  return options()->Enabled(RewriteOptions::kExtendCacheCss);
}

bool RewriteDriver::MayCacheExtendImages() const {
  return options()->Enabled(RewriteOptions::kExtendCacheImages) &&
      !ShouldNotRewriteImages();
}

bool RewriteDriver::MayCacheExtendScripts() const {
  return options()->Enabled(RewriteOptions::kExtendCacheScripts);
}

void RewriteDriver::AddRewriteTask(Function* task) {
  rewrite_worker_->Add(task);
}

void RewriteDriver::AddLowPriorityRewriteTask(Function* task) {
  low_priority_rewrite_worker_->Add(task);
}

OptionsAwareHTTPCacheCallback::OptionsAwareHTTPCacheCallback(
    const RewriteOptions* rewrite_options)
    : cache_invalidation_timestamp_ms_(
          rewrite_options->cache_invalidation_timestamp()) {}

OptionsAwareHTTPCacheCallback::~OptionsAwareHTTPCacheCallback() {}

bool OptionsAwareHTTPCacheCallback::IsCacheValid(
    const ResponseHeaders& headers) {
  return headers.IsDateLaterThan(cache_invalidation_timestamp_ms_);
}

RewriteDriver::CssResolutionStatus RewriteDriver::ResolveCssUrls(
    const GoogleUrl& input_css_base,
    const StringPiece& output_css_base,
    const StringPiece& contents,
    Writer* writer,
    MessageHandler* handler) {
  GoogleUrl output_base(output_css_base);
  bool proxy_mode;
  if (ShouldAbsolutifyUrl(input_css_base, output_base, &proxy_mode)) {
    RewriteDomainTransformer transformer(&input_css_base, &output_base, this);
    if (proxy_mode) {
      // If URLs are being rewritten to a proxy domain, then trimming
      // them based purely on the domain-lawyer mappings is going to
      // relativize them so that they cannot be resolved properly in
      // their intended context.
      //
      // TODO(jmarantz): Consider merging the url_namer with DomainLawyer
      // so that DomainLawyer::WillDomainChange will be accurate.
      transformer.set_trim_urls(false);
    }
    if (CssTagScanner::TransformUrls(contents, writer, &transformer, handler)) {
      return kSuccess;
    } else {
      return kWriteFailed;
    }
  }
  return kNoResolutionNeeded;
}

bool RewriteDriver::ShouldAbsolutifyUrl(const GoogleUrl& input_base,
                                        const GoogleUrl& output_base,
                                        bool* proxy_mode) const {
  bool result = false;
  const UrlNamer* url_namer = resource_manager_->url_namer();
  bool proxying = url_namer->ProxyMode();

  if (proxying) {
    result = !url_namer->IsProxyEncoded(input_base);
  } else if (input_base.AllExceptLeaf() != output_base.AllExceptLeaf()) {
    result = true;
  } else {
    const DomainLawyer* domain_lawyer = options()->domain_lawyer();
    result = domain_lawyer->WillDomainChange(input_base.Origin());
  }

  if (proxy_mode != NULL) {
    *proxy_mode = proxying;
  }

  return result;
}

}  // namespace net_instaweb
