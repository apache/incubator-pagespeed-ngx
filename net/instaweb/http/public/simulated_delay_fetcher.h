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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_SIMULATED_DELAY_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_SIMULATED_DELAY_FETCHER_H_

#include <map>

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

class AbstractMutex;
class AsyncFetch;
class MessageHandler;
class Scheduler;
class ThreadSystem;
class Timer;

// SimulatedDelayFetcher lets one configure various per-host delays, and will
// make hardcoded replies according to those replies. This exists to help run
// simulations of server behavior with sites of widely various speeds.
class SimulatedDelayFetcher : public UrlAsyncFetcher {
 public:
  // The payload that this will deliver.
  static const char kPayload[];

  // delay_map_path is the path to the file describing the delays for each host
  // The format is: foo.com=4;bar.com=42;baz.com=43; (line breaks are permitted)
  // The delays are in milliseconds.
  //
  // request_log_path will be used to log when each request was received.
  // (Not when it was served).
  SimulatedDelayFetcher(ThreadSystem* thread_system,
                        Timer* timer,
                        Scheduler* scheduler,
                        MessageHandler* handler,
                        FileSystem* file_system,
                        StringPiece delay_map_path,
                        StringPiece request_log_path,
                        int request_log_flush_frequency);

  virtual ~SimulatedDelayFetcher();

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

 private:
  typedef std::map<GoogleString, int> DelayMap;
  void ProduceReply(AsyncFetch* fetch);
  void ParseDelayMap(StringPiece delay_map_path);

  Timer* timer_;
  Scheduler* scheduler_;
  MessageHandler* message_handler_;
  FileSystem* file_system_;
  DelayMap delays_ms_;
  int request_log_flush_frequency_;

  scoped_ptr<AbstractMutex> mutex_;
  int request_log_outstanding_ GUARDED_BY(mutex_.get());
  FileSystem::OutputFile* request_log_ PT_GUARDED_BY(mutex_.get());

  DISALLOW_COPY_AND_ASSIGN(SimulatedDelayFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_SIMULATED_DELAY_FETCHER_H_
