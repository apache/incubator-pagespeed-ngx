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
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

// If your .cc file needs to use the types declared in logging_proto.h,
// you must also include net/instaweb/http/public/logging_proto_impl.h
// See that header file for an explanation of why this is necessary.


namespace net_instaweb {

class AbstractMutex;

// This class is a wrapper around a protobuf used to collect logging
// information. It also provides a simple aggregation mechanism for
// collecting the ids of applied rewriters.
//
// Care and feeding of log records:
//  (1) All logging must be done through log records. No class should
//      have static members of any logging proto class. Log records
//      can either create the logging protos, or will take ownership of them.
//  (2) All access and manipulation of log data must be guarded by the log
//      record's mutex. Commonly repeated logging operations should be factored
//      into functions in this class (and be so guarded therein).
//  (3) In most cases, log records should be created and owned by request
//      contexts.

// Subclasses may wrap some other type of protobuf; they must still provide
// access to a LogRecord instance, however.
class LogRecord  {
 public:
  // Construct a LogRecord with a new LoggingInfo proto and caller-
  // supplied mutex. This class takes ownership of the mutex.
  explicit LogRecord(AbstractMutex* mutex);
  virtual ~LogRecord();

  // Log a rewriter (identified by an id string) as having been applied to
  // the request being logged. These ids will be aggregated and written to the
  // protobuf when Finalize() is called.
  void LogAppliedRewriter(const char* rewriter_id);

  // This should be called when all logging activity on the log record is
  // complete. If a subclass of this class uses other aggregate data structures
  // or other intermediates before writing to the wrapped data structure,
  // it should do those writes in FinalizeImpl. mutex_ guards this.
  void Finalize();

  // Return the LoggingInfo proto wrapped by this class. Calling code must
  // guard any reads and writes to this using mutex().
  virtual LoggingInfo* logging_info();

  // Mutex-guarded log mutation convenience methods. The rule of thumb is that
  // if a single-field update to a logging proto occurs multiple times, it
  // should be factored out into a method on this class.
  void SetBlinkRequestFlow(int flow);
  void SetIsOriginalResourceCacheable(bool cacheable);
  void SetTimingRequestStartMs(int64 ms);
  void SetTimingFetchMs(int64 ms);

  // Mutex-guarded log-writing operations. Derived classes should override
  // *Impl methods. Returns false if the log write attempt failed.
  bool WriteLog();
  // Update the log record with Blink-specific information, then write the
  // log as if WriteLog() was called.
  bool WriteLogForBlink(const GoogleString& user_agent);

  // If log-writing needs to occur in the context of an existing lock,
  // these methods may be used. Returns false if write attempt failed.
  bool WriteLogWhileLocked();
  bool WriteLogForBlinkWhileLocked(const GoogleString& user_agent);

  // Return the mutex associated with this instance. Calling code should
  // guard reads and writes of LogRecords
  AbstractMutex* mutex() { return mutex_.get(); }

 protected:
  // Non-initializing default constructor for subclasses. Subclasses that invoke
  // this constructor should implement and call their own initializer that
  // instantiates the wrapped logging proto and calls set_mutex with a valid
  // Mutex object.
  LogRecord();

  void set_mutex(AbstractMutex* m);

  // Returns a comma-joined string concatenating the contents of
  // applied_rewriters_
  GoogleString ConcatenatedRewriterString();

  // Implementation methods for subclasses to override.
  // Implements logging an applied rewriter.
  virtual void LogAppliedRewriterImpl(const char* rewriter_id);
  // Implements finalization.
  virtual void FinalizeImpl();
  // Implements writing a log, base implementation is a no-op. Returns false if
  // writing failed.
  virtual bool WriteLogImpl() { return true; }
  // Implements writing the Blink log, base implementation is a no-op. Returns
  // false if writing failed.
  virtual bool WriteLogForBlinkImpl(const GoogleString& user_agent) {
    return true;
  }

  // True if Finalize() has been called. mutex_ guards this.
  bool finalized() { return finalized_; }
  FRIEND_TEST(LogRecordTest, NoAppliedRewriters);

 private:
  // Called on construction.
  void InitLogging();

  StringSet applied_rewriters_;

  scoped_ptr<LoggingInfo> logging_info_;
  bool finalized_;
  // Thus must be set. Implementation constructors must minimally default this
  // to a NullMutex.
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(LogRecord);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_
