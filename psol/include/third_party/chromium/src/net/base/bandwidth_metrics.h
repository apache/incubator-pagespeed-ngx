// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_BANDWIDTH_METRICS_H_
#define NET_BASE_BANDWIDTH_METRICS_H_
#pragma once

#include <list>

#include "base/metrics/histogram.h"
#include "base/logging.h"
#include "base/time.h"

namespace net {

// Tracks statistics about the bandwidth metrics over time.  In order to
// measure, this class needs to know when individual streams are in progress,
// so that it can know when to discount idle time.  The BandwidthMetrics
// is unidirectional - it should only be used to record upload or download
// bandwidth, but not both.
//
// Note, the easiest thing to do is to just measure each stream and average
// them or add them.  However, this does not work.  If multiple streams are in
// progress concurrently, you have to look at the aggregate bandwidth at any
// point in time.
//
//   Example:
//      Imagine 4 streams opening and closing with overlapping time.
//      We can't measure bandwidth by looking at any individual stream.
//      We can only measure actual bandwidth by looking at the bandwidth
//      across all open streams.
//
//         Time --------------------------------------->
//         s1 +----------------+
//         s2              +----------------+
//         s3                            +--------------+
//         s4                            +--------------+
//
// Example usage:
//
//   BandwidthMetrics tracker;
//
//   // When a stream is created
//   tracker.StartStream();
//
//   // When data is transferred on any stream
//   tracker.RecordSample(bytes);
//
//   // When the stream is finished
//   tracker.StopStream();
//
// NOTE: This class is not thread safe.
//
class BandwidthMetrics {
 public:
  BandwidthMetrics()
      : num_streams_in_progress_(0),
        num_data_samples_(0),
        data_sum_(0.0),
        bytes_since_last_start_(0) {
  }

  // Get the bandwidth.  Returns Kbps (kilo-bits-per-second).
  double bandwidth() const {
    return data_sum_ / num_data_samples_;
  }

  // Record that we've started a stream.
  void StartStream() {
    // If we're the only stream, we've finished some idle time.  Record a new
    // timestamp to indicate the start of data flow.
    if (++num_streams_in_progress_ == 1) {
      last_start_ = base::TimeTicks::HighResNow();
      bytes_since_last_start_ = 0;
    }
  }

  // Track that we've completed a stream.
  void StopStream() {
    if (--num_streams_in_progress_ == 0) {
      // We don't use small streams when tracking bandwidth because they are not
      // precise; imagine a 25 byte stream.  The sample is too small to make
      // a good measurement.
      // 20KB is an arbitrary value.  We might want to use a lesser value.
      static const int64 kRecordSizeThreshold = 20 * 1024;
      if (bytes_since_last_start_ < kRecordSizeThreshold)
        return;

      base::TimeDelta delta = base::TimeTicks::HighResNow() - last_start_;
      double ms = delta.InMillisecondsF();
      if (ms > 0.0) {
        double kbps = static_cast<double>(bytes_since_last_start_) * 8 / ms;
        ++num_data_samples_;
        data_sum_ += kbps;
        LOG(INFO) << "Bandwidth: " << kbps
                  << "Kbps (avg " << bandwidth() << "Kbps)";
        int kbps_int = static_cast<int>(kbps);
        UMA_HISTOGRAM_COUNTS_10000("Net.DownloadBandwidth", kbps_int);
      }
    }
  }

  // Add a sample of the number of bytes read from the network into the tracker.
  void RecordBytes(int bytes) {
    DCHECK(num_streams_in_progress_);
    bytes_since_last_start_ += static_cast<int64>(bytes);
  }

 private:
  int num_streams_in_progress_;   // The number of streams in progress.
  // TODO(mbelshe): Use a rolling buffer of 30 samples instead of an average.
  int num_data_samples_;          // The number of samples collected.
  double data_sum_;               // The sum of all samples collected.
  int64 bytes_since_last_start_;  // Bytes tracked during this "session".
  base::TimeTicks last_start_;    // Timestamp of the begin of this "session".
};

// A utility class for managing the lifecycle of a measured stream.
// It is important that we not leave unclosed streams, and this class helps
// ensure we always stop them.
class ScopedBandwidthMetrics {
 public:
  ScopedBandwidthMetrics();
  ~ScopedBandwidthMetrics();

  void StartStream();
  void StopStream();
  void RecordBytes(int bytes);

 private:
  bool started_;
};

}  // namespace net

#endif  // NET_BASE_BANDWIDTH_METRICS_H_
