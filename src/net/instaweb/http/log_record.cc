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

// Author: marq@google.com (Mark Cogan)

#include "net/instaweb/http/public/log_record.h"

#include <set>

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

LogRecord::LogRecord(LoggingInfo* logging_info) {
  set_logging_info(logging_info);
}

void LogRecord::LogAppliedRewriter(const char* rewriter_id) {
  applied_rewriters_.insert(rewriter_id);
}

void LogRecord::Finalize() {
  logging_info()->set_applied_rewriters(ConcatenatedRewriterString());
}

GoogleString LogRecord::ConcatenatedRewriterString() {
  GoogleString rewriters_str;
  StringSet::iterator iter;
  for (iter = applied_rewriters_.begin(); iter != applied_rewriters_.end();
      ++iter) {
    if (iter != applied_rewriters_.begin()) {
      StrAppend(&rewriters_str, ",");
    }
    StrAppend(&rewriters_str, *iter);
  }
  return rewriters_str;
}


}  // namespace net_instaweb
