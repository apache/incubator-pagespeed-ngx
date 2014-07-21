/*
 * Copyright 2013 Google Inc.
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

// Authors: mmohabey@google.com (Megha Mohabey)
//          pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/automatic/public/cache_html_flow.h"

#include "base/logging.h"
#include "net/instaweb/automatic/public/html_detector.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/cache_html_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/cache_html_info_finder.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
// TODO(pulkitg): Change the function GetHtmlCriticalImages to take
// AbstractPropertyPage so that fallback_property_page.h dependency can be
// removed.
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/http/http.pb.h"

namespace net_instaweb {

const char kBlinkJsString[] =
    "<script type=\"text/javascript\" src=\"%s\"></script>";
const char kCacheHtmlSuffixJsString[] =
    "<script type=\"text/javascript\">"
    "pagespeed.panelLoaderInit();"
    "</script>\n";
const char kCacheHtmlSetInternalIp[] =
    "<script type=\"text/javascript\">"
    "pagespeed.panelLoader.setRequestFromInternalIp();"
    "</script>\n";

const char CacheHtmlFlow::kBackgroundComputationDone[] =
    "BackgroundComputation:Done";
const char CacheHtmlFlow::kNumCacheHtmlHits[] =
    "num_cache_html_hits";
const char CacheHtmlFlow::kNumCacheHtmlMisses[] =
    "num_cache_html_misses";
const char CacheHtmlFlow::kNumCacheHtmlMatches[] =
    "num_cache_html_matches";
const char CacheHtmlFlow::kNumCacheHtmlMismatches[] =
    "num_cache_html_mismatches";
const char CacheHtmlFlow::kNumCacheHtmlMismatchesCacheDeletes[] =
    "num_cache_html_mismatch_cache_deletes";
const char CacheHtmlFlow::kNumCacheHtmlSmartdiffMatches[] =
    "num_cache_html_smart_diff_matches";
const char CacheHtmlFlow::kNumCacheHtmlSmartdiffMismatches[] =
    "num_cache_html_smart_diff_mismatches";

// Utility for logging to both main and cache html flow log records.
// Does not take ownership of the passed in log records.
class CacheHtmlFlow::LogHelper {
 public:
  LogHelper(AbstractLogRecord* log_record1, AbstractLogRecord* log_record2)
      : log_record1_(log_record1), log_record2_(log_record2) {}

  void SetCacheHtmlRequestFlow(int32 cache_html_request_flow) {
    log_record1_->SetCacheHtmlRequestFlow(cache_html_request_flow);
    log_record2_->SetCacheHtmlRequestFlow(cache_html_request_flow);
  }

  void LogAppliedRewriter(const char* filter_id) {
    log_record1_->SetRewriterLoggingStatus(filter_id,
                                           RewriterApplication::APPLIED_OK);
    log_record2_->SetRewriterLoggingStatus(filter_id,
                                           RewriterApplication::APPLIED_OK);
  }

 private:
  AbstractLogRecord* log_record1_;
  AbstractLogRecord* log_record2_;

  DISALLOW_COPY_AND_ASSIGN(LogHelper);
};

namespace {

// Reads requisite info from Property Page. After reading, property page in
// driver is set to NULL, so that no one writes to property cache while
// rewriting cached html.
// TODO(mmohabey): Move the logic of copying properties in rewrite_driver when
// it is cloned.
void InitDriverWithPropertyCacheValues(
    RewriteDriver* cache_html_driver, FallbackPropertyPage* page) {
  // TODO(pulkitg): Change the function GetHtmlCriticalImages to take
  // AbstractPropertyPage as a parameter so that
  // set_unowned_fallback_property_page function call can be removed. Also make
  // the function take AbstractPropertyPage instead of FallbackPropertyPage.
  cache_html_driver->set_unowned_fallback_property_page(page);
  // TODO(mmohabey): Critical line info should be populated here.

  ServerContext* server_context = cache_html_driver->server_context();

  // Because we are resetting the property page at the end of this function, we
  // need to make sure the CriticalImageFinder state is updated here. We don't
  // have a public interface for updating the state in the driver, so perform a
  // throwaway critical image query here, which will in turn cause the state
  // that CriticalImageFinder keeps in RewriteDriver to be updated.
  // TODO(jud): Remove this when the CriticalImageFinder is held in the
  // RewriteDriver, instead of ServerContext.
  server_context->critical_images_finder()->
      GetHtmlCriticalImages(cache_html_driver);

  CriticalSelectorFinder* selector_finder =
      server_context->critical_selector_finder();
  if (selector_finder != NULL) {
    selector_finder->GetCriticalSelectors(cache_html_driver);
  }

  CriticalCssFinder* css_finder = server_context->critical_css_finder();
  if (css_finder != NULL) {
    css_finder->UpdateCriticalCssInfoInDriver(cache_html_driver);
  }

  CacheHtmlInfoFinder* cache_html_finder =
      cache_html_driver->server_context()->cache_html_info_finder();
  if (cache_html_finder != NULL) {
    cache_html_finder->UpdateSplitInfoInDriver(cache_html_driver);
  }

  cache_html_driver->set_unowned_fallback_property_page(NULL);
}

class CacheHtmlComputationFetch : public AsyncFetch {
 public:
  CacheHtmlComputationFetch(const GoogleString& url,
                            RewriteDriver* rewrite_driver,
                            CacheHtmlInfo* cache_html_info,
                            AbstractLogRecord* cache_html_log_record,
                            CacheHtmlFlow::LogHelper* cache_html_log_helper)
      : AsyncFetch(rewrite_driver->request_context()),
        url_(url),
        server_context_(rewrite_driver->server_context()),
        options_(rewrite_driver->options()),
        rewrite_driver_(rewrite_driver),
        cache_html_log_record_(cache_html_log_record),
        cache_html_log_helper_(cache_html_log_helper),
        cache_html_info_(cache_html_info),
        claims_html_(false),
        probable_html_(false),
        content_length_over_threshold_(false),
        non_ok_status_code_(false),
        cache_html_change_mutex_(
            server_context_->thread_system()->NewMutex()),
        finish_(false) {
    // Makes rewrite_driver live longer as ProxyFetch may called Cleanup()
    // on the rewrite_driver even if ComputeCacheHtmlInfo() has not yet
    // been triggered.
    rewrite_driver_->increment_async_events_count();
    Statistics* stats = server_context_->statistics();
    num_cache_html_misses_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlMisses);
    num_cache_html_matches_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlMatches);
    num_cache_html_mismatches_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlMismatches);
    num_cache_html_mismatches_cache_deletes_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlMismatchesCacheDeletes);
    num_cache_html_smart_diff_matches_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlSmartdiffMatches);
    num_cache_html_smart_diff_mismatches_ = stats->GetTimedVariable(
        CacheHtmlFlow::kNumCacheHtmlSmartdiffMismatches);
  }

  virtual ~CacheHtmlComputationFetch() {
    cache_html_log_record_->SetCacheHtmlLoggingInfo("");
    if (!cache_html_log_record_->WriteLog()) {
      LOG(WARNING) <<  "Cache html flow GWS Logging failed for " << url_;
    }
    rewrite_driver_->decrement_async_events_count();
    ThreadSynchronizer* sync = server_context_->thread_synchronizer();
    sync->Signal(CacheHtmlFlow::kBackgroundComputationDone);
  }

  virtual void HandleHeadersComplete() {
    if (response_headers()->status_code() == HttpStatus::kOK) {
      claims_html_ = response_headers()->IsHtmlLike();
      int64 content_length;
      bool content_length_found = response_headers()->FindContentLength(
          &content_length);
      if (content_length_found && content_length >
          options_->blink_max_html_size_rewritable()) {
        content_length_over_threshold_ = true;
      }
    } else {
      non_ok_status_code_ = true;
      VLOG(1) << "Non 200 response code for: " << url_;
    }
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    if (!claims_html_ || content_length_over_threshold_) {
      return true;
    }
    if (!html_detector_.already_decided() &&
        html_detector_.ConsiderInput(content)) {
      if (html_detector_.probable_html()) {
        probable_html_ = true;
        html_detector_.ReleaseBuffered(&buffer_);
      }
    }
    // TODO(poojatandon): share this logic of finding the length and setting a
    // limit with http_cache code.
    if (probable_html_) {
      if (unsigned(buffer_.size() + content.size()) >
          options_->blink_max_html_size_rewritable()) {
        content_length_over_threshold_ = true;
        buffer_.clear();
      } else {
        content.AppendToString(&buffer_);
      }
    }
    return true;
  }

  virtual bool HandleFlush(MessageHandler* handler) {
    return true;
    // No operation.
  }

  virtual void HandleDone(bool success) {
    if (non_ok_status_code_ || !success || !claims_html_ || !probable_html_ ||
        content_length_over_threshold_) {
      if (cache_html_info_->has_cached_html()) {
        // This means it is a cache hit case.  Currently it also means diff is
        // enabled (possibly in logging mode), since CacheHtmlComputationFetch
        // is attached in cache hit case only when diff is enabled.
        // Calling Finish since the deletion of this object needs to be
        // synchronized with HandleDone call in AsyncFetchWithHeadersInhibited,
        // since that class refers to this object.
        Finish();
      } else {
        if (content_length_over_threshold_) {
          cache_html_log_helper_->SetCacheHtmlRequestFlow(
              CacheHtmlLoggingInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD);
        } else if (non_ok_status_code_ || !success) {
          cache_html_log_helper_->SetCacheHtmlRequestFlow(
              CacheHtmlLoggingInfo::CACHE_HTML_MISS_FETCH_NON_OK);
        } else if (!claims_html_ || !probable_html_) {
          cache_html_log_helper_->SetCacheHtmlRequestFlow(
              CacheHtmlLoggingInfo::CACHE_HTML_MISS_FOUND_RESOURCE);
        }
        delete this;
      }
      return;
    }
    if (!cache_html_info_->has_cached_html()) {
      cache_html_log_helper_->SetCacheHtmlRequestFlow(
          CacheHtmlLoggingInfo::CACHE_HTML_MISS_TRIGGERED_REWRITE);
    }
    if ((rewrite_driver_->options()->enable_blink_html_change_detection() ||
         rewrite_driver_->options()->
         enable_blink_html_change_detection_logging()) &&
        server_context_->cache_html_info_finder() != NULL) {
      // We do diff mismatch detection in cache miss case also so that we can
      // update the content hash and smart text hash in CacheHtmlInfo in pcache.
      CreateHtmlChangeDetectionDriverAndRewrite();
    } else {
      CreateCacheHtmlComputationDriverAndRewrite();
    }
  }

  void CreateHtmlChangeDetectionDriverAndRewrite() {
    RewriteOptions* options = rewrite_driver_->options()->Clone();
    options->ClearFilters();
    options->ForceEnableFilter(RewriteOptions::kRemoveComments);
    options->ForceEnableFilter(RewriteOptions::kStripNonCacheable);
    options->ForceEnableFilter(RewriteOptions::kComputeVisibleText);
    server_context_->ComputeSignature(options);
    html_change_detection_driver_ =
        server_context_->NewCustomRewriteDriver(options, request_context());
    value_.Clear();
    html_change_detection_driver_->SetWriter(&value_);
    html_change_detection_driver_->set_response_headers_ptr(response_headers());
    complete_finish_parse_html_change_driver_fn_ = MakeFunction(
        this,
        &CacheHtmlComputationFetch::CompleteFinishParseForHtmlChangeDriver);
    html_change_detection_driver_->AddLowPriorityRewriteTask(
        MakeFunction(
            this, &CacheHtmlComputationFetch::Parse,
            &CacheHtmlComputationFetch::CancelParseForHtmlChangeDriver,
            html_change_detection_driver_,
            complete_finish_parse_html_change_driver_fn_));
  }

  void CreateCacheHtmlComputationDriverAndRewrite() {
    RewriteOptions* options = rewrite_driver_->options()->Clone();
    options->ClearFilters();
    options->ForceEnableFilter(RewriteOptions::kStripNonCacheable);
    cache_html_computation_driver_ =
        server_context_->NewCustomRewriteDriver(options, request_context());
    value_.Clear();
    cache_html_computation_driver_->SetWriter(&value_);
    cache_html_computation_driver_->set_response_headers_ptr(
        response_headers());
    complete_finish_parse_cache_html_driver_fn_ = MakeFunction(
        this, &CacheHtmlComputationFetch::
        CompleteFinishParseForCacheHtmlComputationDriver);
    cache_html_computation_driver_->AddLowPriorityRewriteTask(
        MakeFunction(
            this, &CacheHtmlComputationFetch::Parse,
            &CacheHtmlComputationFetch::
            CancelParseForCacheHtmlComputationDriver,
            cache_html_computation_driver_,
            complete_finish_parse_cache_html_driver_fn_));
  }

  void Parse(RewriteDriver* driver, Function* task) {
    driver->StartParse(url_);
    driver->ParseText(buffer_);
    driver->FinishParseAsync(task);
  }

  void CancelParseForCacheHtmlComputationDriver(RewriteDriver* driver,
                                                   Function* task) {
    LOG(WARNING) << "Cache Html computation dropped due to load"
                 << " for url: " << url_;
    complete_finish_parse_cache_html_driver_fn_->CallCancel();
    cache_html_computation_driver_->Cleanup();
    delete this;
  }

  void CancelParseForHtmlChangeDriver(RewriteDriver* driver, Function* task) {
    LOG(WARNING) << "Html change diff dropped due to load"
                 << " for url: " << url_;
    complete_finish_parse_html_change_driver_fn_->CallCancel();
    html_change_detection_driver_->Cleanup();
    Finish();
  }

  void CompleteFinishParseForCacheHtmlComputationDriver() {
    StringPiece rewritten_content;
    value_.ExtractContents(&rewritten_content);
    cache_html_info_->set_cached_html(rewritten_content.data(),
                                      rewritten_content.size());
    cache_html_info_->set_last_cached_html_computation_timestamp_ms(
        server_context_->timer()->NowMs());
    if (!cache_html_info_->cached_html().empty() &&
        !content_length_over_threshold_) {
      UpdatePropertyCacheWithCacheHtmlInfo();
    }
    delete this;
  }

  void CompleteFinishParseForHtmlChangeDriver() {
    StringPiece output;
    value_.ExtractContents(&output);
    StringPieceVector result;
    net_instaweb::SplitStringUsingSubstr(
        output, BlinkUtil::kComputeVisibleTextFilterOutputEndMarker,
        &result);
    if (result.size() == 2) {
      computed_hash_smart_diff_ = server_context_->hasher()->Hash(result[0]);
      computed_hash_ = server_context_->hasher()->Hash(result[1]);
    }
    if (!cache_html_info_->has_cached_html()) {
      CreateCacheHtmlComputationDriverAndRewrite();
      return;
    }
    {
      ScopedMutex lock(cache_html_log_record_->mutex());
      CacheHtmlLoggingInfo* cache_html_logging_info =
          cache_html_log_record_->
          logging_info()->mutable_cache_html_logging_info();
      if (computed_hash_ != cache_html_info_->hash()) {
        num_cache_html_mismatches_->IncBy(1);
        cache_html_logging_info->set_html_match(false);
      } else {
        num_cache_html_matches_->IncBy(1);
        cache_html_logging_info->set_html_match(true);
      }
      if (computed_hash_smart_diff_ !=
          cache_html_info_->hash_smart_diff()) {
        num_cache_html_smart_diff_mismatches_->IncBy(1);
        cache_html_logging_info->set_html_smart_diff_match(false);
      } else {
        num_cache_html_smart_diff_matches_->IncBy(1);
        cache_html_logging_info->set_html_smart_diff_match(true);
      }
    }
    Finish();
  }

  // This function should only be called if change detection is enabled and
  // this is a cache hit case. In such cases, the content may need to be deleted
  // from the property cache if a change was detected. This deletion should wait
  // for AsyncFetchWithHeadersInhibited to complete (HandleDone called) to
  // ensure that we do not delete entry from cache while it is still being used
  // to process the request.
  //
  // This method achieves this goal using a mutex protected
  // variable finish_. Both CacheHtmlComputationFetch and
  // AsyncFetchWithHeadersInhibited call this method once their processing is
  // done. The first call sets the value of finish_ to true and returns.
  // The second call to this method actually calls ProcessDiffResult.
  void Finish() {
    {
      ScopedMutex lock(cache_html_change_mutex_.get());
      if (!finish_) {
        finish_ = true;
        return;
      }
    }
    ProcessDiffResult();
  }

  // This method processes the result of html change detection. If a mismatch
  // is found, we delete the entry from the cache and trigger a cache html info
  // computation.
  void ProcessDiffResult() {
    if (computed_hash_.empty()) {
      delete this;
      return;
    }
    bool compute_cache_html_info = false;
    if (options_->use_smart_diff_in_blink()) {
      compute_cache_html_info =
          (computed_hash_smart_diff_ !=
              cache_html_info_->hash_smart_diff());
    } else {
      compute_cache_html_info =
          (computed_hash_ !=
              cache_html_info_->hash());
    }

    int64 now_ms = server_context_->timer()->NowMs();
    PropertyPage* page = rewrite_driver_->property_page();
    const PropertyCache::Cohort* cohort = server_context_->blink_cohort();
    bool diff_info_updated =
        server_context_->cache_html_info_finder()->UpdateDiffInfo(
            compute_cache_html_info, now_ms, cache_html_log_record_.get(),
            rewrite_driver_, server_context_->factory());

    if (options_->enable_blink_html_change_detection() &&
        compute_cache_html_info) {
      num_cache_html_mismatches_cache_deletes_->IncBy(1);
      // TODO(mmohabey): Do not call delete here as we will be subsequently
      // updating the new value in property cache using
      // CreateCacheHtmlComputationDriverAndRewrite.
      server_context_->cache_html_info_finder()->PropagateCacheDeletes(
          url_,
          rewrite_driver_->options()->experiment_id(),
          rewrite_driver_->device_type());
      page->DeleteProperty(
          cohort, BlinkUtil::kCacheHtmlRewriterInfo);
      page->WriteCohort(cohort);
      CreateCacheHtmlComputationDriverAndRewrite();
    } else if (options_->enable_blink_html_change_detection() ||
               computed_hash_ != cache_html_info_->hash() ||
               computed_hash_smart_diff_ !=
               cache_html_info_->hash_smart_diff()) {
      UpdatePropertyCacheWithCacheHtmlInfo();
      delete this;
    } else {
      if (diff_info_updated) {
        page->WriteCohort(cohort);
      }
      delete this;
    }
  }

  void UpdatePropertyCacheWithCacheHtmlInfo() {
    cache_html_info_->set_charset(response_headers()->DetermineCharset());
    cache_html_info_->set_hash(computed_hash_);
    cache_html_info_->set_hash_smart_diff(computed_hash_smart_diff_);

    UpdateInPropertyCache(*cache_html_info_, rewrite_driver_,
                          server_context_->blink_cohort(),
                          BlinkUtil::kCacheHtmlRewriterInfo,
                          true /* write_cohort*/);
  }


 private:
  GoogleString url_;
  ServerContext* server_context_;
  const RewriteOptions* options_;
  GoogleString buffer_;
  HTTPValue value_;
  HtmlDetector html_detector_;
  GoogleString computed_hash_;
  GoogleString computed_hash_smart_diff_;
  HttpResponseHeaders http_response_headers_;

  // RewriteDriver passed to ProxyFetch to serve user-facing request.
  RewriteDriver* rewrite_driver_;
  // RewriteDriver used to parse the buffered html content.
  RewriteDriver* cache_html_computation_driver_;
  RewriteDriver* html_change_detection_driver_;
  scoped_ptr<AbstractLogRecord> cache_html_log_record_;
  scoped_ptr<CacheHtmlFlow::LogHelper> cache_html_log_helper_;
  scoped_ptr<CacheHtmlInfo> cache_html_info_;
  Function* complete_finish_parse_cache_html_driver_fn_;
  Function* complete_finish_parse_html_change_driver_fn_;
  bool claims_html_;
  bool probable_html_;
  bool content_length_over_threshold_;
  bool non_ok_status_code_;

  // Variables to manage change detection processing.
  // Mutex
  scoped_ptr<AbstractMutex> cache_html_change_mutex_;
  bool finish_;  // protected by cache_html_change_mutex_

  TimedVariable* num_cache_html_misses_;
  TimedVariable* num_cache_html_matches_;
  TimedVariable* num_cache_html_mismatches_;
  TimedVariable* num_cache_html_mismatches_cache_deletes_;
  TimedVariable* num_cache_html_smart_diff_matches_;
  TimedVariable* num_cache_html_smart_diff_mismatches_;

  DISALLOW_COPY_AND_ASSIGN(CacheHtmlComputationFetch);
};

// AsyncFetch that doesn't call HeadersComplete() on the base fetch. Note that
// this class only links the request headers from the base fetch and does not
// link the response headers.
// This is used as a wrapper around the base fetch when CacheHtmlInfo is
// found in cache. This is done because the response headers and the
// cached html have been already been flushed out in the base fetch
// and we don't want to call HeadersComplete() twice on the base fetch.
// This class deletes itself when HandleDone() is called.
class AsyncFetchWithHeadersInhibited : public AsyncFetchUsingWriter {
 public:
  AsyncFetchWithHeadersInhibited(
      AsyncFetch* fetch,
      CacheHtmlComputationFetch* cache_html_computation_fetch)
      : AsyncFetchUsingWriter(fetch->request_context(), fetch),
        base_fetch_(fetch),
        cache_html_computation_fetch_(cache_html_computation_fetch) {
    set_request_headers(fetch->request_headers());
  }

 private:
  virtual ~AsyncFetchWithHeadersInhibited() {
  }

  virtual void HandleHeadersComplete() {}

  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
    if (cache_html_computation_fetch_ != NULL) {
      cache_html_computation_fetch_->Finish();
    }
    delete this;
  }

  AsyncFetch* base_fetch_;
  CacheHtmlComputationFetch* cache_html_computation_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithHeadersInhibited);
};

}  // namespace

void CacheHtmlFlow::Start(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback) {
  CacheHtmlFlow* flow = new CacheHtmlFlow(
      url, base_fetch, driver, factory, property_cache_callback);

  Function* func = MakeFunction(flow, &CacheHtmlFlow::CacheHtmlLookupDone,
                                &CacheHtmlFlow::Cancel);
  property_cache_callback->AddPostLookupTask(func);

  // Not doing any config lookup until pcache completes.
  property_cache_callback->RequestHeadersComplete();
}

void CacheHtmlFlow::InitStats(Statistics* stats) {
  stats->AddTimedVariable(kNumCacheHtmlHits,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlMisses,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlMatches,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlMismatches,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlMismatchesCacheDeletes,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlSmartdiffMatches,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumCacheHtmlSmartdiffMismatches,
                          ServerContext::kStatisticsGroup);
}

CacheHtmlFlow::CacheHtmlFlow(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback)
    : url_(url),
      google_url_(url),
      base_fetch_(base_fetch),
      cache_html_log_record_(
          base_fetch_->request_context()->NewSubordinateLogRecord(
              new NullMutex)),
      rewrite_driver_(driver),
      options_(driver->options()),
      factory_(factory),
      server_context_(driver->server_context()),
      property_cache_callback_(property_cache_callback),
      handler_(rewrite_driver_->server_context()->message_handler()),
      cache_html_log_helper_(new LogHelper(
          cache_html_log_record_.get(),
          base_fetch_->request_context()->log_record())) {
  Statistics* stats = server_context_->statistics();
  num_cache_html_misses_ = stats->GetTimedVariable(
      kNumCacheHtmlMisses);
  num_cache_html_hits_ = stats->GetTimedVariable(
      kNumCacheHtmlHits);
  const char* request_event_id = base_fetch_->request_headers()->Lookup1(
      HttpAttributes::kXGoogleRequestEventId);
  {
    ScopedMutex lock(cache_html_log_record_->mutex());
    CacheHtmlLoggingInfo* cache_html_logging_info =
        cache_html_log_record_->
        logging_info()->mutable_cache_html_logging_info();
    cache_html_logging_info->set_url(url_);
    if (request_event_id != NULL) {
      cache_html_logging_info->set_request_event_id_time_usec(request_event_id);
    }
  }
}

CacheHtmlFlow::~CacheHtmlFlow() {
}

void CacheHtmlFlow::CacheHtmlLookupDone() {
  FallbackPropertyPage* fallback_page =
      property_cache_callback_->fallback_property_page();
  PopulateCacheHtmlInfo(fallback_page->actual_property_page());

  // TODO(mmohabey): Add CSI timings.
  if (cache_html_info_.has_cached_html()) {
    CacheHtmlHit(fallback_page);
  } else {
    CacheHtmlMiss();
  }
}

void CacheHtmlFlow::CacheHtmlMiss() {
  num_cache_html_misses_->IncBy(1);
  TriggerProxyFetch();
}

void CacheHtmlFlow::CacheHtmlHit(FallbackPropertyPage* page) {
  num_cache_html_hits_->IncBy(1);
  StringPiece cached_html = cache_html_info_.cached_html();
  // TODO(mmohabey): Handle malformed html case.
  cache_html_log_helper_->SetCacheHtmlRequestFlow(
      CacheHtmlLoggingInfo::CACHE_HTML_HIT);
  cache_html_log_helper_->LogAppliedRewriter(
            RewriteOptions::FilterId(RewriteOptions::kCachePartialHtml));

  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->SetStatusAndReason(HttpStatus::kOK);
  // TODO(pulkitg): Store content type in pcache.
  // TODO(mmohabey): Handle Meta tags.
  GoogleString content_type = StrCat(
      "text/html", cache_html_info_.has_charset() ?
      StrCat("; charset=", cache_html_info_.charset()) : "");
  response_headers->Add(HttpAttributes::kContentType, content_type);
  {
    ScopedMutex lock(cache_html_log_record_->mutex());
    response_headers->Add(
       kPsaRewriterHeader, cache_html_log_record_->AppliedRewritersString());
  }
  response_headers->ComputeCaching();
  response_headers->SetDateAndCaching(server_context_->timer()->NowMs(), 0,
                                      ", private, no-cache");
  // If relevant, add the Set-Cookie header for experiments.
  if (options_->need_to_store_experiment_data() &&
      options_->running_experiment()) {
    int experiment_value = options_->experiment_id();
    server_context_->experiment_matcher()->StoreExperimentData(
        experiment_value, url_,
        server_context_->timer()->NowMs() +
            options_->experiment_cookie_duration_ms(),
        response_headers);
  }
  base_fetch_->HeadersComplete();

  // Clone the RewriteDriver which is used to rewrite the HTML that we are
  // trying to flush early.
  RewriteDriver* new_driver = rewrite_driver_->Clone();
  new_driver->set_response_headers_ptr(base_fetch_->response_headers());
  new_driver->set_flushing_cached_html(true);
  new_driver->SetWriter(base_fetch_);
  new_driver->SetUserAgent(rewrite_driver_->user_agent());
  new_driver->StartParse(url_);

  InitDriverWithPropertyCacheValues(new_driver, page);

  bool flushed_split_js =
      new_driver->options()->Enabled(RewriteOptions::kSplitHtml) &&
      new_driver->request_properties()->SupportsSplitHtml(
          new_driver->options()->enable_aggressive_rewriters_for_mobile());
  new_driver->ParseText(cached_html);
  new_driver->FinishParseAsync(
      MakeFunction(this, &CacheHtmlFlow::CacheHtmlRewriteDone,
                   flushed_split_js));
}

void CacheHtmlFlow::CacheHtmlRewriteDone(bool flushed_split_js) {
  rewrite_driver_->set_flushed_cached_html(true);

  StaticAssetManager* static_asset_manager =
      server_context_->static_asset_manager();
  if (!flushed_split_js) {
    base_fetch_->Write(StringPrintf(kBlinkJsString,
        static_asset_manager->GetAssetUrl(
            StaticAssetManager::kBlinkJs, options_).c_str()), handler_);
    base_fetch_->Write(kCacheHtmlSuffixJsString, handler_);
  }
  const char* user_ip = base_fetch_->request_headers()->Lookup1(
      HttpAttributes::kXForwardedFor);
  if (user_ip != NULL && server_context_->factory()->IsDebugClient(user_ip) &&
      options_->enable_blink_debug_dashboard()) {
    base_fetch_->Write(kCacheHtmlSetInternalIp, handler_);
  }
  base_fetch_->Flush(handler_);
  TriggerProxyFetch();
}

void CacheHtmlFlow::TriggerProxyFetch() {
  bool flushed_cached_html = rewrite_driver_->flushed_cached_html();
  AsyncFetch* fetch = NULL;
  CacheHtmlComputationFetch* cache_html_computation_fetch = NULL;

  // Remove any headers that can lead to a 304, since CacheHtmlFlow can't
  // handle 304s.
  base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfNoneMatch);
  base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfModifiedSince);

  if (!flushed_cached_html || options_->enable_blink_html_change_detection() ||
      options_->enable_blink_html_change_detection_logging()) {
    CacheHtmlInfo* cache_html_info = new CacheHtmlInfo();
    cache_html_info->CopyFrom(cache_html_info_);
    cache_html_computation_fetch = new CacheHtmlComputationFetch(
        url_, rewrite_driver_, cache_html_info,
        cache_html_log_record_.release(), cache_html_log_helper_.release());
    // TODO(mmohabey) : Set a fixed user agent for fetching content from the
    // origin server if options->use_fixed_user_agent_for_blink_cache_misses()
    // is enabled.
  }

  if (flushed_cached_html) {
    // TODO(mmohabey): Disable LazyloadImages filter for the driver sending non
    // cacheables.
    fetch = new AsyncFetchWithHeadersInhibited(base_fetch_,
                                               cache_html_computation_fetch);
  } else {
    // PassThrough case.
    // This flow has side effect that DeferJs is applied in passthrough case
    // even when it is not explicitly enabled since it is added in
    // RewriteDriver::AddPostRenderFilters() if
    // RewriteOptions::kCachePartialHtml is enabled.
    fetch = base_fetch_;
  }
  if (cache_html_computation_fetch == NULL) {
    cache_html_log_record_->SetCacheHtmlLoggingInfo(
        fetch->request_headers()->Lookup1(HttpAttributes::kUserAgent));
    if (!cache_html_log_record_->WriteLog()) {
      LOG(ERROR) <<  "Cache html flow GWS Logging failed for " << url_;
    }
  }  // else, logging will be done by cache_html_computation_fetch.
  factory_->StartNewProxyFetch(
            url_, fetch, rewrite_driver_, property_cache_callback_,
            cache_html_computation_fetch);
  delete this;
}

// TODO(mmohabey): Disable conflicting filters for cache html flow.

void CacheHtmlFlow::Cancel() {
  delete this;
}

void CacheHtmlFlow::PopulateCacheHtmlInfo(PropertyPage* page) {
  const PropertyCache::Cohort* cohort = server_context_->page_property_cache()->
      GetCohort(BlinkUtil::kBlinkCohort);
  if (page == NULL || cohort == NULL) {
    return;
  }

  PropertyValue* property_value = page->GetProperty(
      cohort, BlinkUtil::kCacheHtmlRewriterInfo);
  if (!property_value->has_value()) {
    return;
  }
  ArrayInputStream value(property_value->value().data(),
                         property_value->value().size());
  if (!cache_html_info_.ParseFromZeroCopyStream(&value)) {
    LOG(DFATAL) << "Parsing value from cache into CacheHtmlInfo failed.";
    cache_html_info_.Clear();
  }
  return;
}

}  // namespace net_instaweb
