// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SampleVector implements HistogramSamples interface. It is used by all
// Histogram based classes to store samples.

#ifndef BASE_METRICS_SAMPLE_VECTOR_H_
#define BASE_METRICS_SAMPLE_VECTOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"

namespace base {

class BucketRanges;

class BASE_EXPORT_PRIVATE SampleVector : public HistogramSamples {
 public:
  explicit SampleVector(const BucketRanges* bucket_ranges);
  virtual ~SampleVector();

  // HistogramSamples implementation:
  virtual void Accumulate(HistogramBase::Sample value,
                          HistogramBase::Count count) OVERRIDE;
  virtual HistogramBase::Count GetCount(
      HistogramBase::Sample value) const OVERRIDE;
  virtual HistogramBase::Count TotalCount() const OVERRIDE;
  virtual scoped_ptr<SampleCountIterator> Iterator() const OVERRIDE;

  // Get count of a specific bucket.
  HistogramBase::Count GetCountAtIndex(size_t bucket_index) const;

 protected:
  virtual bool AddSubtractImpl(
      SampleCountIterator* iter,
      HistogramSamples::Operator op) OVERRIDE;  // |op| is ADD or SUBTRACT.

  virtual size_t GetBucketIndex(HistogramBase::Sample value) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptSampleCounts);

  std::vector<HistogramBase::Count> counts_;

  // Shares the same BucketRanges with Histogram object.
  const BucketRanges* const bucket_ranges_;

  DISALLOW_COPY_AND_ASSIGN(SampleVector);
};

class BASE_EXPORT_PRIVATE SampleVectorIterator : public SampleCountIterator {
 public:
  SampleVectorIterator(const std::vector<HistogramBase::Count>* counts,
                       const BucketRanges* bucket_ranges);
  virtual ~SampleVectorIterator();

  // SampleCountIterator implementation:
  virtual bool Done() const OVERRIDE;
  virtual void Next() OVERRIDE;
  virtual void Get(HistogramBase::Sample* min,
                   HistogramBase::Sample* max,
                   HistogramBase::Count* count) const OVERRIDE;

  // SampleVector uses predefined buckets, so iterator can return bucket index.
  virtual bool GetBucketIndex(size_t* index) const OVERRIDE;

 private:
  void SkipEmptyBuckets();

  const std::vector<HistogramBase::Count>* counts_;
  const BucketRanges* bucket_ranges_;

  size_t index_;
};

}  // namespace base

#endif  // BASE_METRICS_SAMPLE_VECTOR_H_
