// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_FILTERS_RESPONSE_BYTE_RESULT_FILTER_H_
#define PAGESPEED_FILTERS_RESPONSE_BYTE_RESULT_FILTER_H_

#include "pagespeed/core/engine.h"

namespace pagespeed {

class Result;

class ResponseByteResultFilter : public ResultFilter {
 public:
  // The default threshold for this filter.
  static const int kDefaultThresholdBytes = 256;

  // Construct a ResponseByteResultFilter with the given
  // threshold. Results that have a response byte savings less than
  // the specified threshold will not be accepted.
  explicit ResponseByteResultFilter(int threshold);

  // Construct a ResponseByteResultFilter with the default threshold.
  ResponseByteResultFilter();
  virtual ~ResponseByteResultFilter();

  virtual bool IsAccepted(const Result& result) const;

 private:
  int response_byte_threshold_;

  DISALLOW_COPY_AND_ASSIGN(ResponseByteResultFilter);
};

}

#endif  // PAGESPEED_FILTERS_RESPONSE_BYTE_RESULT_FILTER_H_

