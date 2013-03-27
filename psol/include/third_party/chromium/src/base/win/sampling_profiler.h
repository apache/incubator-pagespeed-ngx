// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SAMPLING_PROFILER_H_
#define BASE_WIN_SAMPLING_PROFILER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace win {

// This class exposes the functionality of Window's built-in sampling profiler.
// Each profiler instance covers a range of memory, and while the profiler is
// running, its buckets will count the number of times the instruction counter
// lands in the associated range of memory on a sample.
// The sampling interval is settable, but the setting is system-wide.
class BASE_EXPORT SamplingProfiler {
 public:
  // Create an uninitialized sampling profiler.
  SamplingProfiler();
  ~SamplingProfiler();

  // Initializes the profiler to cover the memory range |start| through
  // |start| + |size|, in the process |process_handle| with bucket size
  // |2^log2_bucket_size|, |log2_bucket_size| must be in the range 2-31,
  // for bucket sizes of 4 bytes to 2 gigabytes.
  // The process handle must grant at least PROCESS_QUERY_INFORMATION.
  // The memory range should be exectuable code, like e.g. the text segment
  // of an exectuable (whether DLL or EXE).
  // Returns true on success.
  bool Initialize(HANDLE process_handle,
                  void* start,
                  size_t size,
                  size_t log2_bucket_size);

  // Start this profiler, which must be initialized and not started.
  bool Start();
  // Stop this profiler, which must be started.
  bool Stop();

  // Get and set the sampling interval.
  // Note that this is a system-wide setting.
  static bool SetSamplingInterval(base::TimeDelta sampling_interval);
  static bool GetSamplingInterval(base::TimeDelta* sampling_interval);

  // Accessors.
  bool is_started() const { return is_started_; }

  // It is safe to read the counts in the sampling buckets at any time.
  // Note however that there's no guarantee that you'll read consistent counts
  // until the profiler has been stopped, as the counts may be updating on other
  // CPU cores.
  const std::vector<ULONG>& buckets() const { return buckets_; }

 private:
  // Handle to the corresponding kernel object.
  ScopedHandle profile_handle_;
  // True iff this profiler is started.
  bool is_started_;
  std::vector<ULONG> buckets_;

  DISALLOW_COPY_AND_ASSIGN(SamplingProfiler);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SAMPLING_PROFILER_H_
