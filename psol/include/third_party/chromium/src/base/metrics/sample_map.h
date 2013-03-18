// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SampleMap implements HistogramSamples interface. It is used by the
// SparseHistogram class to store samples.

#ifndef BASE_METRICS_SAMPLE_MAP_H_
#define BASE_METRICS_SAMPLE_MAP_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"

namespace base {

class BASE_EXPORT_PRIVATE SampleMap : public HistogramSamples {
 public:
  SampleMap();
  virtual ~SampleMap();

  // HistogramSamples implementation:
  virtual void Accumulate(HistogramBase::Sample value,
                          HistogramBase::Count count) OVERRIDE;
  virtual HistogramBase::Count GetCount(
      HistogramBase::Sample value) const OVERRIDE;
  virtual HistogramBase::Count TotalCount() const OVERRIDE;
  virtual scoped_ptr<SampleCountIterator> Iterator() const OVERRIDE;

  void ResetRedundantCount(HistogramBase::Count count);

 protected:
  virtual bool AddSubtractImpl(
      SampleCountIterator* iter,
      HistogramSamples::Operator op) OVERRIDE;  // |op| is ADD or SUBTRACT.

 private:
  std::map<HistogramBase::Sample, HistogramBase::Count> sample_counts_;

  DISALLOW_COPY_AND_ASSIGN(SampleMap);
};

class BASE_EXPORT_PRIVATE SampleMapIterator : public SampleCountIterator {
 public:
  typedef std::map<HistogramBase::Sample, HistogramBase::Count>
      SampleToCountMap;

  SampleMapIterator(const SampleToCountMap& sample_counts);
  virtual ~SampleMapIterator();

  // SampleCountIterator implementation:
  virtual bool Done() const OVERRIDE;
  virtual void Next() OVERRIDE;
  virtual void Get(HistogramBase::Sample* min,
                   HistogramBase::Sample* max,
                   HistogramBase::Count* count) const OVERRIDE;
 private:
  SampleToCountMap::const_iterator iter_;
  const SampleToCountMap::const_iterator end_;
};

}  // namespace base

#endif  // BASE_METRICS_SAMPLE_MAP_H_
