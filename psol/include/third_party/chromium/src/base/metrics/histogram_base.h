// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_BASE_H_
#define BASE_METRICS_HISTOGRAM_BASE_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {

class DictionaryValue;
class ListValue;

class BASE_EXPORT HistogramBase {
 public:
  typedef int Sample;  // Used for samples.
  typedef int Count;   // Used to count samples.

  static const Sample kSampleType_MAX;  // INT_MAX

  enum Flags {
    kNoFlags = 0,
    kUmaTargetedHistogramFlag = 0x1,  // Histogram should be UMA uploaded.

    // Indicate that the histogram was pickled to be sent across an IPC Channel.
    // If we observe this flag on a histogram being aggregated into after IPC,
    // then we are running in a single process mode, and the aggregation should
    // not take place (as we would be aggregating back into the source
    // histogram!).
    kIPCSerializationSourceFlag = 0x10,

    // Only for Histogram and its sub classes: fancy bucket-naming support.
    kHexRangePrintingFlag = 0x8000,
  };


  HistogramBase(const std::string& name);
  virtual ~HistogramBase();

  std::string histogram_name() const { return histogram_name_; }

  // Operations with Flags enum.
  int32 flags() const { return flags_; }
  void SetFlags(int32 flags);
  void ClearFlags(int32 flags);

  virtual void Add(Sample value) = 0;

  // The following methods provide graphical histogram displays.
  virtual void WriteHTMLGraph(std::string* output) const = 0;
  virtual void WriteAscii(std::string* output) const = 0;

  // Produce a JSON representation of the histogram. This is implemented with
  // the help of GetParameters and GetCountAndBucketData; overwrite them to
  // customize the output.
  void WriteJSON(std::string* output) const;

protected:
  // Writes information about the construction parameters in |params|.
  virtual void GetParameters(DictionaryValue* params) const = 0;

  // Writes information about the current (non-empty) buckets and their sample
  // counts to |buckets| and the total sample count to |count|.
  virtual void GetCountAndBucketData(Count* count,
                                     ListValue* buckets) const = 0;
 private:
  const std::string histogram_name_;
  int32 flags_;

  DISALLOW_COPY_AND_ASSIGN(HistogramBase);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_BASE_H_
