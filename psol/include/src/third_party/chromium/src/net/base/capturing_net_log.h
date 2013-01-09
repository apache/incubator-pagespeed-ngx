// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CAPTURING_NET_LOG_H_
#define NET_BASE_CAPTURING_NET_LOG_H_
#pragma once

#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "net/base/net_api.h"
#include "net/base/net_log.h"

namespace net {

// CapturingNetLog is an implementation of NetLog that saves messages to a
// bounded buffer.
class NET_API CapturingNetLog : public NetLog {
 public:
  struct NET_API Entry {
    Entry(EventType type,
          const base::TimeTicks& time,
          Source source,
          EventPhase phase,
          EventParameters* extra_parameters);
    ~Entry();

    EventType type;
    base::TimeTicks time;
    Source source;
    EventPhase phase;
    scoped_refptr<EventParameters> extra_parameters;
  };

  // Ordered set of entries that were logged.
  typedef std::vector<Entry> EntryList;

  enum { kUnbounded = -1 };

  // Creates a CapturingNetLog that logs a maximum of |max_num_entries|
  // messages.
  explicit CapturingNetLog(size_t max_num_entries);
  virtual ~CapturingNetLog();

  // Returns the list of all entries in the log.
  void GetEntries(EntryList* entry_list) const;

  void Clear();

  void SetLogLevel(NetLog::LogLevel log_level);

  // NetLog implementation:
  virtual void AddEntry(EventType type,
                        const base::TimeTicks& time,
                        const Source& source,
                        EventPhase phase,
                        EventParameters* extra_parameters);
  virtual uint32 NextID();
  virtual LogLevel GetLogLevel() const;

 private:
  // Needs to be "mutable" so can use it in GetEntries().
  mutable base::Lock lock_;

  // Last assigned source ID.  Incremented to get the next one.
  base::subtle::Atomic32 last_id_;

  size_t max_num_entries_;
  EntryList entries_;

  NetLog::LogLevel log_level_;

  DISALLOW_COPY_AND_ASSIGN(CapturingNetLog);
};

// Helper class that exposes a similar API as BoundNetLog, but uses a
// CapturingNetLog rather than the more generic NetLog.
//
// CapturingBoundNetLog can easily be converted to a BoundNetLog using the
// bound() method.
class NET_TEST CapturingBoundNetLog {
 public:
  CapturingBoundNetLog(const NetLog::Source& source, CapturingNetLog* net_log);

  explicit CapturingBoundNetLog(size_t max_num_entries);

  ~CapturingBoundNetLog();

  // The returned BoundNetLog is only valid while |this| is alive.
  BoundNetLog bound() const {
    return BoundNetLog(source_, capturing_net_log_.get());
  }

  // Fills |entry_list| with all entries in the log.
  void GetEntries(CapturingNetLog::EntryList* entry_list) const;

  void Clear();

  // Sets the log level of the underlying CapturingNetLog.
  void SetLogLevel(NetLog::LogLevel log_level);

 private:
  NetLog::Source source_;
  scoped_ptr<CapturingNetLog> capturing_net_log_;

  DISALLOW_COPY_AND_ASSIGN(CapturingBoundNetLog);
};

}  // namespace net

#endif  // NET_BASE_CAPTURING_NET_LOG_H_

