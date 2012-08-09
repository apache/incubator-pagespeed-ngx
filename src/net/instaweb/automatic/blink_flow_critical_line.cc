/*
 * Copyright 2012 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)
//
// This class manages the flow of a blink request. In order to flush the
// critical html early before we start getting bytes back from the
// fetcher, we lookup property cache for BlinkCriticalLineData.
// If found, we flush critical html out and then trigger the normal
// ProxyFetch flow with customized options which extracts cookies and
// non cacheable panels from the page and sends it out.
// If BlinkCriticalLineData is not found in cache, we pass this request through
// normal ProxyFetch flow buffering the html. In the background we
// create a driver to parse it, run it through other filters, compute
// BlinkCriticalLineData and store it into the property cache.

#include "net/instaweb/automatic/public/blink_flow_critical_line.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/automatic/public/html_detector.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/logging.pb.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/blink_critical_line_data.pb.h"
#include "net/instaweb/rewriter/public/blink_critical_line_data_finder.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;

const char BlinkFlowCriticalLine::kBackgroundComputationDone[] =
    "BackgroundComputation:Done";
const char BlinkFlowCriticalLine::kHtmlDiffComputationMatched[] =
    "HtmlDiffComputation:Matched";
const char BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits[] =
    "num_blink_html_cache_hits";
const char BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses[] =
    "num_blink_html_cache_misses";
const char BlinkFlowCriticalLine::kNumBlinkSharedFetchesStarted[] =
    "num_blink_shared_fetches_started";
const char BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted[] =
    "num_blink_shared_fetches_completed";
const char BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls[] =
    "num_compute_blink_critical_line_data_calls";
const char BlinkFlowCriticalLine::kNumBlinkHtmlMatches[] =
    "num_blink_html_matches";
const char BlinkFlowCriticalLine::kNumBlinkHtmlMismatches[] =
    "num_blink_html_mismatches";
const char BlinkFlowCriticalLine::kAboveTheFold[] = "Above the fold";

namespace {

const char kTimeToBlinkFlowStart[] = "BLINK_FLOW_START";
const char kTimeToBlinkDataLookUpDone[] = "BLINK_DATA_LOOK_UP_DONE";

// AsyncFetch that doesn't call HeadersComplete() on the base fetch. Note that
// this class only links the request headers from the base fetch and does not
// link the response headers.
// This is used as a wrapper around the base fetch when BlinkCriticalLineData is
// found in cache. This is done because the response headers and the
// BlinkCriticalLineData have been already been flushed out in the base fetch
// and we don't want to call HeadersComplete() twice on the base fetch.
// This class deletes itself when HandleDone() is called.
class AsyncFetchWithHeadersInhibited : public AsyncFetchUsingWriter {
 public:
  explicit AsyncFetchWithHeadersInhibited(AsyncFetch* fetch)
      : AsyncFetchUsingWriter(fetch),
        base_fetch_(fetch) {
    set_request_headers(fetch->request_headers());
  }

 private:
  virtual ~AsyncFetchWithHeadersInhibited() {}
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
    delete this;
  }

  AsyncFetch* base_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithHeadersInhibited);
};

// AsyncFetch that gets the original fetched content, determines if the content
// is html and then decides whether to trigger an async computation of the
// critical line data.
// If html change detection is enabled, it also diffs the
// incoming html hash with the stored hash. If the hash has changed, then also
// triggers critical line data computation.
// TODO(rahulbansal): Buffer the html chunked rather than in one string.
class CriticalLineFetch : public AsyncFetch {
 public:
  CriticalLineFetch(const GoogleString& url,
                    ResourceManager* resource_manager,
                    RewriteOptions* options,
                    RewriteDriver* rewrite_driver,
                    BlinkInfo* blink_info,
                    BlinkCriticalLineData* blink_critical_line_data)
      : url_(url),
        resource_manager_(resource_manager),
        options_(options),
        rewrite_driver_(rewrite_driver),
        blink_info_(blink_info),
        blink_critical_line_data_(blink_critical_line_data),
        claims_html_(false),
        probable_html_(false),
        content_length_over_threshold_(false),
        non_ok_status_code_(false) {
    // Makes rewrite_driver live longer as ProxyFetch may called Cleanup()
    // on the rewrite_driver even if ComputeBlinkCriticalLineData() has not yet
    // been triggered.
    rewrite_driver_->increment_async_events_count();
    Statistics* stats = resource_manager->statistics();
    num_blink_html_cache_misses_ = stats->GetTimedVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses);
    num_compute_blink_critical_line_data_calls_ = stats->GetTimedVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls);
    num_blink_shared_fetches_completed_ = stats->GetTimedVariable(
        BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted);
    num_blink_html_matches_ = stats->GetTimedVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMatches);
    num_blink_html_mismatches_ = stats->GetTimedVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatches);
  }

  virtual ~CriticalLineFetch() {
    rewrite_driver_->decrement_async_events_count();
    ThreadSynchronizer* sync = resource_manager_->thread_synchronizer();
    sync->Signal(BlinkFlowCriticalLine::kBackgroundComputationDone);
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
    num_blink_shared_fetches_completed_->IncBy(1);
    if (non_ok_status_code_ || !success || !claims_html_ || !probable_html_ ||
        content_length_over_threshold_) {
      if (content_length_over_threshold_) {
        blink_info_->set_blink_request_flow(
            BlinkInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD);
      } else if (non_ok_status_code_ || !success) {
        blink_info_->set_blink_request_flow(
            BlinkInfo::BLINK_CACHE_MISS_FETCH_NON_OK);
      } else if (!claims_html_ || !probable_html_) {
        blink_info_->set_blink_request_flow(
            BlinkInfo::BLINK_CACHE_MISS_FOUND_RESOURCE);
      }
      delete this;
      return;
    }
    blink_info_->set_blink_request_flow(
        BlinkInfo::BLINK_CACHE_MISS_TRIGGERED_REWRITE);
    if (rewrite_driver_->options()->
        passthrough_blink_for_last_invalid_response_code()) {
      rewrite_driver_->UpdatePropertyValueInDomCohort(
          BlinkUtil::kBlinkResponseCodePropertyName,
          IntegerToString(response_headers()->status_code()));
    }

    if (options_->enable_blink_html_change_detection()) {
      // We'll reach here only in case of Cache Hit case.
      CreateHtmlChangeDetectionDriverAndRewrite();
    } else {
      CreateCriticalLineComputationDriverAndRewrite("");
    }
  }

  void CreateHtmlChangeDetectionDriverAndRewrite() {
    RewriteOptions* options = options_->Clone();
    options->ClearFilters();
    options->ForceEnableFilter(RewriteOptions::kRemoveComments);
    options->ForceEnableFilter(RewriteOptions::kStripNonCacheable);
    resource_manager_->ComputeSignature(options);
    html_change_detection_driver_ =
        resource_manager_->NewCustomRewriteDriver(options);
    value_.Clear();
    html_change_detection_driver_->SetWriter(&value_);
    html_change_detection_driver_->set_response_headers_ptr(response_headers());
    complete_finish_parse_html_change_driver_fn_ = MakeFunction(
        this, &CriticalLineFetch::CompleteFinishParseForHtmlChangeDriver);
    html_change_detection_driver_->AddLowPriorityRewriteTask(
        MakeFunction(
            this, &CriticalLineFetch::Parse,
            &CriticalLineFetch::CancelParseForHtmlChangeDriver,
            html_change_detection_driver_,
            complete_finish_parse_html_change_driver_fn_));
  }

  void CreateCriticalLineComputationDriverAndRewrite(
      const GoogleString& computed_hash) {
    num_blink_html_cache_misses_->IncBy(1);
    critical_line_computation_driver_ =
        resource_manager_->NewCustomRewriteDriver(options_.release());
    // Wait for all rewrites to complete. This is important because fully
    // rewritten html is used to compute BlinkCriticalLineData.
    critical_line_computation_driver_->set_fully_rewrite_on_flush(true);
    value_.Clear();
    critical_line_computation_driver_->SetWriter(&value_);
    critical_line_computation_driver_->set_response_headers_ptr(
        response_headers());
    complete_finish_parse_critical_line_driver_fn_ = MakeFunction(
        this, &CriticalLineFetch::CompleteFinishParseForCriticalLineDriver,
        computed_hash);
    critical_line_computation_driver_->AddLowPriorityRewriteTask(
        MakeFunction(
            this, &CriticalLineFetch::Parse,
            &CriticalLineFetch::CancelParseForCriticalLineComputationDriver,
            critical_line_computation_driver_,
            complete_finish_parse_critical_line_driver_fn_));
  }

  void Parse(RewriteDriver* driver, Function* task) {
    if (driver->StartParse(url_)) {
      driver->ParseText(buffer_);
      driver->FinishParseAsync(task);
    } else {
      LOG(ERROR) << "StartParse failed for url: " << url_;
      driver->Cleanup();
      delete this;
    }
  }

  void CancelParseForCriticalLineComputationDriver() {
    LOG(WARNING) << "Blink critical line computation dropped due to load"
                 << " for url: " << url_;
    complete_finish_parse_critical_line_driver_fn_->CallCancel();
    critical_line_computation_driver_->Cleanup();
    delete this;
  }

  void CancelParseForHtmlChangeDriver() {
    LOG(WARNING) << "Blink html change diff dropped due to load"
                 << " for url: " << url_;
    complete_finish_parse_html_change_driver_fn_->CallCancel();
    html_change_detection_driver_->Cleanup();
    delete this;
  }

  void CompleteFinishParseForCriticalLineDriver(GoogleString computed_hash) {
    StringPiece rewritten_content;
    value_.ExtractContents(&rewritten_content);
    num_compute_blink_critical_line_data_calls_->IncBy(1);
    resource_manager_->blink_critical_line_data_finder()
        ->ComputeBlinkCriticalLineData(computed_hash, rewritten_content,
                                       response_headers(), rewrite_driver_);
    delete this;
  }

  void CompleteFinishParseForHtmlChangeDriver() {
    StringPiece rewritten_content;
    value_.ExtractContents(&rewritten_content);
    GoogleString computed_hash =
        resource_manager_->hasher()->Hash(rewritten_content);
    // We invoke critical line computation in case of cache miss or html hash
    // mismatch.
    if (blink_critical_line_data_ == NULL ||
        computed_hash != blink_critical_line_data_->hash()) {
      num_blink_html_mismatches_->IncBy(1);
      CreateCriticalLineComputationDriverAndRewrite(computed_hash);
    } else {
      num_blink_html_matches_->IncBy(1);
      blink_critical_line_data_->set_last_diff_timestamp_ms(
          resource_manager_->timer()->NowMs());

      // TODO(rahulbansal): Move the code to write to pcache to blink_util.cc
      PropertyCache* property_cache =
          rewrite_driver_->resource_manager()->page_property_cache();
      PropertyPage* page = rewrite_driver_->property_page();
      const PropertyCache::Cohort* cohort = property_cache->GetCohort(
          BlinkUtil::kBlinkCohort);
      GoogleString buf;
      blink_critical_line_data_->SerializeToString(&buf);
      PropertyValue* property_value = page->GetProperty(
          cohort, BlinkUtil::kBlinkCriticalLineDataPropertyName);
      property_cache->UpdateValue(buf, property_value);
      property_cache->WriteCohort(cohort, page);
      ThreadSynchronizer* sync = resource_manager_->thread_synchronizer();
      sync->Signal(BlinkFlowCriticalLine::kHtmlDiffComputationMatched);
      delete this;
    }
  }

 private:
  GoogleString url_;
  ResourceManager* resource_manager_;
  scoped_ptr<RewriteOptions> options_;
  GoogleString buffer_;
  HTTPValue value_;
  HtmlDetector html_detector_;

  // RewriteDriver passed to ProxyFetch to serve user-facing request.
  RewriteDriver* rewrite_driver_;
  // RewriteDriver used to parse the buffered html content.
  RewriteDriver* critical_line_computation_driver_;
  RewriteDriver* html_change_detection_driver_;
  BlinkInfo* blink_info_;
  scoped_ptr<BlinkCriticalLineData> blink_critical_line_data_;
  Function* complete_finish_parse_critical_line_driver_fn_;
  Function* complete_finish_parse_html_change_driver_fn_;
  bool claims_html_;
  bool probable_html_;
  bool content_length_over_threshold_;
  bool non_ok_status_code_;

  TimedVariable* num_blink_html_cache_misses_;
  TimedVariable* num_blink_shared_fetches_completed_;
  TimedVariable* num_compute_blink_critical_line_data_calls_;
  TimedVariable* num_blink_html_matches_;
  TimedVariable* num_blink_html_mismatches_;

  DISALLOW_COPY_AND_ASSIGN(CriticalLineFetch);
};

// SharedAsyncFetch that only updates property cache with response code.  Used
// in the case of a cache hit with last response code not OK.
class UpdateResponseCodeSharedAyncFetch : public SharedAsyncFetch {
 public:
  UpdateResponseCodeSharedAyncFetch(AsyncFetch* base_fetch,
                                    RewriteDriver* rewrite_driver)
      : SharedAsyncFetch(base_fetch),
        rewrite_driver_(rewrite_driver),
        updated_response_code_(false) {
    rewrite_driver_->increment_async_events_count();
  }

  virtual ~UpdateResponseCodeSharedAyncFetch() {
    rewrite_driver_->decrement_async_events_count();
  }

 protected:
  virtual bool HandleWrite(const StringPiece& str,
                           MessageHandler* message_handler) {
    bool ret = SharedAsyncFetch::HandleWrite(str, message_handler);
    if (!updated_response_code_ &&
        rewrite_driver_->property_page() != NULL) {
      updated_response_code_ = true;
      rewrite_driver_->UpdatePropertyValueInDomCohort(
          BlinkUtil::kBlinkResponseCodePropertyName,
          IntegerToString(response_headers()->status_code()));
    }
    return ret;
  }

  virtual void HandleDone(bool success) {
    SharedAsyncFetch::HandleDone(success);
    delete this;
  }

 private:
  RewriteDriver* rewrite_driver_;  // We do not own this.
  bool updated_response_code_;

  DISALLOW_COPY_AND_ASSIGN(UpdateResponseCodeSharedAyncFetch);
};

}  // namespace

void BlinkFlowCriticalLine::Start(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteOptions* options,
    ProxyFetchFactory* factory,
    ResourceManager* manager,
    ProxyFetchPropertyCallbackCollector* property_callback) {
  BlinkFlowCriticalLine* flow = new BlinkFlowCriticalLine(
      url, base_fetch, options, factory, manager, property_callback);
  flow->SetStartRequestTimings();
  flow->SetResponseStartTime();
  Function* func = MakeFunction(
      flow, &BlinkFlowCriticalLine::BlinkCriticalLineDataLookupDone,
      property_callback);
  property_callback->AddPostLookupTask(func);
}

void BlinkFlowCriticalLine::SetStartRequestTimings() {
  TimingInfo timing_info = base_fetch_->logging_info()->timing_info();
  if (timing_info.has_request_start_ms()) {
    request_start_time_ms_ = timing_info.request_start_ms();
  } else {
    request_start_time_ms_ = manager_->timer()->NowMs();
  }
}

void BlinkFlowCriticalLine::SetResponseStartTime() {
  time_to_start_blink_flow_critical_line_ms_ =
      GetTimeElapsedFromStartRequest();
}

BlinkFlowCriticalLine::~BlinkFlowCriticalLine() {
}

void BlinkFlowCriticalLine::Initialize(Statistics* stats) {
  stats->AddTimedVariable(kNumBlinkHtmlCacheHits,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumBlinkHtmlCacheMisses,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumBlinkSharedFetchesStarted,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumBlinkSharedFetchesCompleted,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumComputeBlinkCriticalLineDataCalls,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumBlinkHtmlMatches,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumBlinkHtmlMismatches,
                          ResourceManager::kStatisticsGroup);
}

BlinkFlowCriticalLine::BlinkFlowCriticalLine(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteOptions* options,
    ProxyFetchFactory* factory,
    ResourceManager* manager,
    ProxyFetchPropertyCallbackCollector* property_callback)
    : url_(url),
      google_url_(url),
      base_fetch_(base_fetch),
      blink_info_(base_fetch_->logging_info()->mutable_blink_info()),
      options_(options),
      factory_(factory),
      manager_(manager),
      property_callback_(property_callback),
      finder_(manager->blink_critical_line_data_finder()),
      request_start_time_ms_(-1),
      time_to_start_blink_flow_critical_line_ms_(-1),
      time_to_critical_line_data_look_up_done_ms_(-1) {
  Statistics* stats = manager_->statistics();
  num_blink_html_cache_hits_ = stats->GetTimedVariable(
      kNumBlinkHtmlCacheHits);
  num_blink_shared_fetches_started_ = stats->GetTimedVariable(
      kNumBlinkSharedFetchesStarted);
}

void BlinkFlowCriticalLine::BlinkCriticalLineDataLookupDone(
    ProxyFetchPropertyCallbackCollector* collector) {
  PropertyPage* page = collector->GetPropertyPageWithoutOwnership(
      ProxyFetchPropertyCallback::kPagePropertyCache);
  time_to_critical_line_data_look_up_done_ms_ =
      GetTimeElapsedFromStartRequest();
  // finder_ will be never NULL because it is checked before entering
  // BlinkFlowCriticalLine.
  blink_critical_line_data_.reset(finder_->ExtractBlinkCriticalLineData(
      options_->GetBlinkCacheTimeFor(google_url_), page));

  if (blink_critical_line_data_ != NULL &&
      !(options_->passthrough_blink_for_last_invalid_response_code() &&
        IsLastResponseCodeInvalid(page))) {
    BlinkCriticalLineDataHit();
    return;
  }
  if (options_->passthrough_blink_for_last_invalid_response_code() &&
        IsLastResponseCodeInvalid(page)) {
    blink_info_->set_blink_request_flow(
        BlinkInfo::FOUND_LAST_STATUS_CODE_NON_OK);
  }
  BlinkCriticalLineDataMiss();
}

void BlinkFlowCriticalLine::BlinkCriticalLineDataMiss() {
  TriggerProxyFetch(false, false);
}

bool BlinkFlowCriticalLine::IsLastResponseCodeInvalid(PropertyPage* page) {
  const PropertyCache::Cohort* cohort =
    manager_->page_property_cache()->GetCohort(RewriteDriver::kDomCohort);
  if (cohort == NULL) {
    // If dom cohort is not available then we do not want to invalidate cache
    // hits based on response code check.
    return false;
  }
  PropertyValue* property_value = page->GetProperty(
      cohort, BlinkUtil::kBlinkResponseCodePropertyName);

  // TODO(rahulbansal): Use stability here.
  if (!property_value->has_value() ||
      property_value->value() == IntegerToString(HttpStatus::kOK)) {
    return false;
  }
  return true;
}

void BlinkFlowCriticalLine::BlinkCriticalLineDataHit() {
  num_blink_html_cache_hits_->IncBy(1);

  const GoogleString& critical_html =
      blink_critical_line_data_->critical_html();
  size_t start_body_pos = critical_html.find(BlinkUtil::kStartBodyMarker);
  size_t end_body_pos = critical_html.rfind(BlinkUtil::kEndBodyTag);
  if (start_body_pos == StringPiece::npos ||
      end_body_pos == StringPiece::npos) {
    LOG(ERROR) << "Marker not found for url " << url_;
    VLOG(1) << "Critical html without marker is " << critical_html;
    blink_info_->set_blink_request_flow(BlinkInfo::FOUND_MALFORMED_HTML);
    BlinkCriticalLineDataMiss();
    return;
  }
  blink_info_->set_blink_request_flow(BlinkInfo::BLINK_CACHE_HIT);
  GoogleUrl* url_with_psa_off = google_url_.CopyAndAddQueryParam(
      RewriteQuery::kModPagespeed, "off");
  const int start_body_marker_length = strlen(BlinkUtil::kStartBodyMarker);
  GoogleString url_str(url_with_psa_off->Spec().data(),
                       url_with_psa_off->Spec().size());
  critical_html_ = StrCat(
      critical_html.substr(0, start_body_pos),
      StringPrintf(
          kNoScriptRedirectFormatter, url_str.c_str(), url_str.c_str()),
      critical_html.substr(start_body_pos + start_body_marker_length,
                           end_body_pos -
                           (start_body_pos + start_body_marker_length)));
  delete url_with_psa_off;

  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->SetStatusAndReason(HttpStatus::kOK);
  // TODO(pulkitg): Store content type in pcache.
  // TODO(guptaa): Send response in source encoding to avoid inconsistencies and
  // response bloating.
  // Setting the charset as utf-8 since thats that output we get from webkit.
  response_headers->Add(HttpAttributes::kContentType,
                        "text/html; charset=utf-8");
  response_headers->Add(kPsaRewriterHeader,
                        BlinkFlowCriticalLine::kAboveTheFold);
  response_headers->ComputeCaching();
  response_headers->SetDateAndCaching(manager_->timer()->NowMs(), 0,
                                      ", private, no-cache");
  base_fetch_->HeadersComplete();

  bool non_cacheable_present =
      !options_->GetBlinkNonCacheableElementsFor(google_url_).empty();

  if (!non_cacheable_present) {
    ServeAllPanelContents();
  } else {
    ServeCriticalPanelContents();
  }

  TriggerProxyFetch(true, non_cacheable_present);
}

void BlinkFlowCriticalLine::ServeAllPanelContents() {
  ServeCriticalPanelContents();
  GoogleString non_critical_json_str =
      blink_critical_line_data_->non_critical_json();
  SendNonCriticalJson(&non_critical_json_str);
}

void BlinkFlowCriticalLine::WriteResponseStartAndLookUpTimings() {
  WriteString(
      GetAddTimingScriptString(kTimeToBlinkFlowStart,
                               time_to_start_blink_flow_critical_line_ms_));
  WriteString(
      GetAddTimingScriptString(kTimeToBlinkDataLookUpDone,
                               time_to_critical_line_data_look_up_done_ms_));
  Flush();
}
void BlinkFlowCriticalLine::ServeCriticalPanelContents() {
  const GoogleString& pushed_images_str =
      blink_critical_line_data_->critical_images_map();
  SendCriticalHtml(critical_html_);
  WriteResponseStartAndLookUpTimings();
  SendInlineImagesJson(pushed_images_str);
}

void BlinkFlowCriticalLine::SendCriticalHtml(
    const GoogleString& critical_html) {
  WriteString(critical_html);
  WriteString("<script>pagespeed.panelLoaderInit();</script>");
  const char* user_ip = base_fetch_->request_headers()->Lookup1(
      HttpAttributes::kXForwardedFor);
  if (user_ip != NULL && manager_->factory()->IsDebugClient(user_ip) &&
      options_->enable_blink_debug_dashboard()) {
    WriteString("<script>pagespeed.panelLoader.setRequestFromInternalIp();"
                "</script>");
  }
  if (!options_->enable_blink_debug_dashboard()) {
    WriteString("<script>"
                "pagespeed.panelLoader.setCsiTimingsReportingEnabled(false);"
                "</script>");
  }
  WriteString("<script>pagespeed.panelLoader.loadCriticalData({});</script>");
  Flush();
}

void BlinkFlowCriticalLine::SendInlineImagesJson(
    const GoogleString& pushed_images_str) {
  WriteString("<script>pagespeed.panelLoader.loadImagesData(");
  WriteString(pushed_images_str);
  WriteString(");</script>");
  Flush();
}

void BlinkFlowCriticalLine::SendNonCriticalJson(
    GoogleString* non_critical_json_str) {
  WriteString("<script>pagespeed.panelLoader.bufferNonCriticalData(");
  BlinkUtil::EscapeString(non_critical_json_str);
  WriteString(*non_critical_json_str);
  WriteString(");</script>");
  Flush();
}

void BlinkFlowCriticalLine::WriteString(const StringPiece& str) {
  base_fetch_->Write(str, manager_->message_handler());
}

GoogleString BlinkFlowCriticalLine::GetAddTimingScriptString(
    const GoogleString& timing_str, int64 time_ms) {
  return StrCat("<script>pagespeed.panelLoader.addCsiTiming(\"", timing_str,
                "\", ", Integer64ToString(time_ms), ")</script>");
}

int64 BlinkFlowCriticalLine::GetTimeElapsedFromStartRequest() {
  return manager_->timer()->NowMs() - request_start_time_ms_;
}

void BlinkFlowCriticalLine::Flush() {
  base_fetch_->Flush(manager_->message_handler());
}

void BlinkFlowCriticalLine::TriggerProxyFetch(bool critical_line_data_found,
                                              bool serve_non_critical) {
  AsyncFetch* fetch = NULL;
  AsyncFetch* secondary_fetch = NULL;
  RewriteOptions* options = NULL;
  RewriteDriver* driver = NULL;

  // Disable filters which trigger render requests. This is not needed for
  // when we have non-200 code but we just blanket disable here.
  options_->DisableFilter(RewriteOptions::kLazyloadImages);
  options_->DisableFilter(RewriteOptions::kDelayImages);
  options_->DisableFilter(RewriteOptions::kInlineImages);

  if (critical_line_data_found) {
    SetFilterOptions(options_);
    options_->ForceEnableFilter(RewriteOptions::kServeNonCacheableNonCritical);
    manager_->ComputeSignature(options_);
    driver = manager_->NewCustomRewriteDriver(options_);

    // Remove any headers that can lead to a 304, since blink can't handle 304s.
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfNoneMatch);
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfModifiedSince);
    // Pass a new fetch into proxy fetch that inhibits HeadersComplete() on the
    // base fetch. It also doesn't attach the response headers from the base
    // fetch since headers have already been flushed out.
    fetch = new AsyncFetchWithHeadersInhibited(base_fetch_);
    bool revalidate_data = options_->enable_blink_html_change_detection() &&
        (manager_->timer()->NowMs() -
            blink_critical_line_data_->last_diff_timestamp_ms() >
            options_->blink_html_change_detection_time_ms());
    if (revalidate_data) {
      options = options_->Clone();
      BlinkCriticalLineData* blink_critical_line_data =
          new BlinkCriticalLineData();
      blink_critical_line_data->MergeFrom(*blink_critical_line_data_);
      options->ForceEnableFilter(RewriteOptions::kStripNonCacheable);
      options->ForceEnableFilter(RewriteOptions::kProcessBlinkInBackground);
      options->DisableFilter(RewriteOptions::kServeNonCacheableNonCritical);
      secondary_fetch = new CriticalLineFetch(url_, manager_, options, driver,
          blink_info_, blink_critical_line_data);
    }
  } else if (blink_critical_line_data_ == NULL) {
    options = options_->Clone();
    SetFilterOptions(options);
    options->ForceEnableFilter(RewriteOptions::kStripNonCacheable);
    options->ForceEnableFilter(RewriteOptions::kProcessBlinkInBackground);
    fetch = base_fetch_;
    manager_->ComputeSignature(options_);
    driver = manager_->NewCustomRewriteDriver(options_);
    num_blink_shared_fetches_started_->IncBy(1);
    secondary_fetch = new CriticalLineFetch(url_, manager_, options, driver,
        blink_info_, NULL);

    // Setting a fixed user-agent for fetching content from origin server.
    if (options->use_fixed_user_agent_for_blink_cache_misses()) {
      base_fetch_->request_headers()->RemoveAll(HttpAttributes::kUserAgent);
      base_fetch_->request_headers()->Add(
          HttpAttributes::kUserAgent,
          options->blink_desktop_user_agent());
    }
  } else {
    // Non 200 status code and Malformed HTML case.
    // TODO(srihari):  Write system tests for this.  This will require a test
    // harness where we can vary the response (status code) for the url being
    // fetched.
    manager_->ComputeSignature(options_);
    driver = manager_->NewCustomRewriteDriver(options_);
    if (options_->passthrough_blink_for_last_invalid_response_code()) {
      fetch = new UpdateResponseCodeSharedAyncFetch(base_fetch_, driver);
    } else {
      fetch = base_fetch_;
    }
  }
  driver->set_is_blink_request(true);  // Mark this as a blink request.
  driver->set_serve_blink_non_critical(serve_non_critical);
  factory_->StartNewProxyFetch(
      url_, fetch, driver, property_callback_, secondary_fetch);
  delete this;
}

void BlinkFlowCriticalLine::SetFilterOptions(RewriteOptions* options) const {
  options->DisableFilter(RewriteOptions::kCombineCss);
  options->DisableFilter(RewriteOptions::kCombineJavascript);
  options->DisableFilter(RewriteOptions::kMoveCssToHead);
  // TODO(rahulbansal): ConvertMetaTags is a special case incompatible filter
  // which actually causes a SIGSEGV.
  options->DisableFilter(RewriteOptions::kConvertMetaTags);
  options->DisableFilter(RewriteOptions::kDeferJavascript);
  options->DisableFilter(RewriteOptions::kDelayImages);
  options->DisableFilter(RewriteOptions::kFlushSubresources);

  options->ForceEnableFilter(RewriteOptions::kDisableJavascript);

  options->set_min_image_size_low_resolution_bytes(0);
  // Enable inlining for all the images in html.
  options->set_max_inlined_preview_images_index(-1);
}

}  // namespace net_instaweb
