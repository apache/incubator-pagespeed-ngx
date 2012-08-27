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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_
#define NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_

#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"


#include "net/instaweb/http/logging.pb.h"


namespace net_instaweb {

// This class is a wrapper around a protobuf used to collect logging
// information. It also provides a simple aggregation mechanism for
// collecting the ids of applied rewriters.

// Subclasses may wrap some other type of protobuf; they must still provide
// access to a LogRecord instance, however.
class LogRecord  {
 public:
  // Construct a LogRecord with a new, owned LoggingInfo proto.
  LogRecord();

  // Construct a LogRecord with an existing LoggingInfo proto, which remains
  // owned by the calling code.
  explicit LogRecord(LoggingInfo* logging_info);

  virtual ~LogRecord();

  // Log a rewriter (identified by an id string) as having been applied to
  // the request being logged. These ids will be aggregated and written to the
  // protobuf when Finalize() is called.
  virtual void LogAppliedRewriter(const char* rewriter_id);

  // This should be called when all logging activity on the log record is
  // complete. If a subclass of this class uses other aggregate data structures
  // or other intermediates before writing to the wrapped data structure,
  // it should do those writes here.
  virtual void Finalize();

  // Return the LoggingInfo proto wrapped by this class.
  virtual LoggingInfo* logging_info() { return logging_info_; }

  virtual void WriteLogForBlink();

 protected:
  // Returns a comma-joined string concatenating the contents of
  // applied_rewriters_
  GoogleString ConcatenatedRewriterString();

 private:
  StringSet applied_rewriters_;

  // This cannot be NULL. Implementation constructors must set this.
  LoggingInfo* logging_info_;
  bool owns_logging_info_;

  DISALLOW_COPY_AND_ASSIGN(LogRecord);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_
