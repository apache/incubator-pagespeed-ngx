/*
 * Copyright 2014 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/http/public/simulated_delay_fetcher.h"

#include <utility>

#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

const char SimulatedDelayFetcher::kPayload[] = "A very complex webpage";

// SimulatedDelayFetcher lets one configure various per-host delays, and will
// make hardcoded replies according to those replies. This exists to help run
// simulations of server behavior with sites of widely various speeds.
SimulatedDelayFetcher::SimulatedDelayFetcher(
    ThreadSystem* thread_system,
    Timer* timer,
    Scheduler* scheduler,
    MessageHandler* handler,
    FileSystem* file_system,
    StringPiece delay_map_path,
    StringPiece request_log_path,
    int request_log_flush_frequency)
    : timer_(timer),
      scheduler_(scheduler),
      message_handler_(handler),
      file_system_(file_system),
      request_log_flush_frequency_(request_log_flush_frequency),
      mutex_(thread_system->NewMutex()),
      request_log_outstanding_(0),
      request_log_(
          file_system_->OpenOutputFile(request_log_path.as_string().c_str(),
                                       message_handler_)) {
  ParseDelayMap(delay_map_path);
}

SimulatedDelayFetcher::~SimulatedDelayFetcher() {
  file_system_->Close(request_log_, message_handler_);
}

void SimulatedDelayFetcher::Fetch(const GoogleString& url,
                                  MessageHandler* message_handler,
                                  AsyncFetch* fetch) {
  fetch = EnableInflation(fetch);

  GoogleUrl gurl(url);
  if (!gurl.IsWebValid()) {
    fetch->Done(false);
    return;
  }

  GoogleString host = gurl.Host().as_string();
  DelayMap::iterator delay = delays_ms_.find(host);
  if (delay == delays_ms_.end()) {
    message_handler_->Message(
        kWarning, "Host not in delay map:%s", host.c_str());
    fetch->Done(false);
  } else {
    int64 now_ms = timer_->NowMs();
    // Log the request.
    GoogleString timestamp;
    ConvertTimeToString(now_ms, &timestamp);

    GoogleString log_msg = StrCat(timestamp, " ", url, "\n");

    {
      ScopedMutex lock(mutex_.get());
      request_log_->Write(log_msg, message_handler_);
      ++request_log_outstanding_;
      if (request_log_outstanding_ >= request_log_flush_frequency_) {
        request_log_->Flush(message_handler_);
        request_log_outstanding_ = 0;
      }
    }

    // delay->second is in milliseconds. It's 'second' as in
    // the thing after first, not the unit of time.
    int64 delay_ms = delay->second;

    scheduler_->AddAlarmAtUs(
        (now_ms + delay_ms) * Timer::kMsUs,
        MakeFunction(this, &SimulatedDelayFetcher::ProduceReply, fetch));
  }
}

void SimulatedDelayFetcher::ProduceReply(AsyncFetch* fetch) {
  fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  fetch->response_headers()->SetDateAndCaching(timer_->NowMs(),
                                               0 /* uncacheable */);
  fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");
  fetch->Write(kPayload, message_handler_);
  fetch->Done(true);
}

void SimulatedDelayFetcher::ParseDelayMap(StringPiece delay_map_path) {
  GoogleString contents;
  if (!file_system_->ReadFile(delay_map_path.as_string().c_str(),
                              &contents,
                              message_handler_)) {
    return;
  }

  // We expect something like host1=123;host2=345; with whitespace between
  // tokens ignored.
  StringPieceVector pairs;
  SplitStringUsingSubstr(contents, ";", &pairs);
  for (int i = 0, n = pairs.size(); i < n; ++i) {
    TrimWhitespace(&pairs[i]);
    if (pairs[i].empty()) {
      continue;
    }
    StringPieceVector host_delay;
    SplitStringUsingSubstr(pairs[i], "=", &host_delay);
    if (host_delay.size() != 2) {
      message_handler_->Message(
          kWarning, "Unable to parse host=delay spec:%s",
          pairs[i].as_string().c_str());
      continue;
    }
    TrimWhitespace(&host_delay[0]);
    TrimWhitespace(&host_delay[1]);
    int delay_ms = 0;
    if (!StringToInt(host_delay[1], &delay_ms)) {
      message_handler_->Message(
          kWarning, "Unable to parse delay spec:%s",
          host_delay[1].as_string().c_str());
      continue;
    }
    delays_ms_[host_delay[0].as_string()] = delay_ms;
  }
}

}  // namespace net_instaweb
