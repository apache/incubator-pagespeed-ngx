// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
#define BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"

namespace base {

class HistogramSamples;
class HistogramFlattener;

// HistogramSnapshotManager handles the logistics of gathering up available
// histograms for recording either to disk or for transmission (such as from
// renderer to browser, or from browser to UMA upload). Since histograms can sit
// in memory for an extended period of time, and are vulnerable to memory
// corruption, this class also validates as much rendundancy as it can before
// calling for the marginal change (a.k.a., delta) in a histogram to be
// recorded.
class BASE_EXPORT HistogramSnapshotManager {
 public:
  explicit HistogramSnapshotManager(HistogramFlattener* histogram_flattener);
  virtual ~HistogramSnapshotManager();

  // Snapshot all histograms, and ask |histogram_flattener_| to record the
  // delta. The arguments allow selecting only a subset of histograms for
  // recording, or to set a flag in each recorded histogram.
  void PrepareDeltas(Histogram::Flags flags_to_set, bool record_only_uma);

 private:
  // Snapshot this histogram, and record the delta.
  void PrepareDelta(const Histogram& histogram);

  // Try to detect and fix count inconsistency of logged samples.
  void InspectLoggedSamplesInconsistency(
      const HistogramSamples& new_snapshot,
      HistogramSamples* logged_samples);

  // For histograms, track what we've already recorded (as a sample for
  // each histogram) so that we can record only the delta with the next log.
  std::map<std::string, HistogramSamples*> logged_samples_;

  // List of histograms found to be corrupt, and their problems.
  std::map<std::string, int> inconsistencies_;

  // |histogram_flattener_| handles the logistics of recording the histogram
  // deltas.
  HistogramFlattener* histogram_flattener_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(HistogramSnapshotManager);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
