// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_FILTERS_TRACKER_FILTER_H_
#define PAGESPEED_FILTERS_TRACKER_FILTER_H_

#include "base/basictypes.h"
#include "pagespeed/filters/url_regex_filter.h"

namespace pagespeed {

/**
 * A ResourceFilter that filters out trackers.
 */
class TrackerFilter : public UrlRegexFilter {
 public:
  TrackerFilter();
  virtual ~TrackerFilter();

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackerFilter);
};

}  // namespace pagespeed

#endif  // PAGESPEED_FILTERS_TRACKER_FILTER_H_
