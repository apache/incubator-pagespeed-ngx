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

#ifndef PAGESPEED_FILTERS_URL_REGEX_FILTER_H_
#define PAGESPEED_FILTERS_URL_REGEX_FILTER_H_

#include "base/basictypes.h"
#include "pagespeed/core/resource_filter.h"
#include "pagespeed/util/regex.h"

namespace pagespeed {

/**
 * A ResourceFilter that filters URLs based on a regular expression. A
 * URL that matches the regular expression is NOT accepted.
 */
class UrlRegexFilter : public ResourceFilter {
 public:
  explicit UrlRegexFilter(const char* url_regex);
  virtual ~UrlRegexFilter();

  virtual bool IsAccepted(const Resource& resource) const;

 private:
  RE url_regex_;

  DISALLOW_COPY_AND_ASSIGN(UrlRegexFilter);
};

}  // namespace pagespeed

#endif  // PAGESPEED_FILTERS_URL_REGEX_FILTER_H_
