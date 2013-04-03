/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: slamm@google.com (Stephen Lamm)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_FINDER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CriticalCssResult;
class PropertyValue;
class RewriteDriver;
class Statistics;
class TimedVariable;


// Finds critical CSS rules (i.e. CSS needed for the initial page load).
class CriticalCssFinder {
 public:
  static const char kCriticalCssValidCount[];
  static const char kCriticalCssExpiredCount[];
  static const char kCriticalCssNotFoundCount[];
  static const char kCriticalCssPropertyName[];

  explicit CriticalCssFinder(Statistics* stats);
  virtual ~CriticalCssFinder();

  static void InitStats(Statistics* statistics);

  // Get critical css result from property cache.
  // Ownership of the result is passed to the caller.
  virtual CriticalCssResult* GetCriticalCssFromCache(RewriteDriver* driver);

  // Compute the critical css for |url|.
  virtual void ComputeCriticalCss(StringPiece url, RewriteDriver* driver) = 0;

  // Copy |critical_css_map| into property cache. Returns true on success.
  virtual bool UpdateCache(RewriteDriver* driver,
                           const CriticalCssResult& result);

  virtual const char* GetCohort() const = 0;

 protected:
  PropertyValue* GetPropertyValue(RewriteDriver* driver);

 private:
  TimedVariable* critical_css_valid_count_;
  TimedVariable* critical_css_expired_count_;
  TimedVariable* critical_css_not_found_count_;

  DISALLOW_COPY_AND_ASSIGN(CriticalCssFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_FINDER_H_
