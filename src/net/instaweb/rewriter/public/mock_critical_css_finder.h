/*
 * Copyright 2012 Google Inc.
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
// Author: ksimbili@google.com (Kishore Simbili)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_CSS_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_CSS_FINDER_H_

#include <cstddef>

#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class RewriteDriver;
class CriticalCssResult;
class Statistics;

// Mock implementation of CriticalCssFinder that can store and retrieve
// critical css proto. Note that this doesn't use property cache.
class MockCriticalCssFinder : public CriticalCssFinder {
 public:
  MockCriticalCssFinder(RewriteDriver* driver, Statistics* stats)
      : CriticalCssFinder(NULL, stats), driver_(driver) {}
  virtual ~MockCriticalCssFinder();

  void AddCriticalCss(const StringPiece& url, const StringPiece& rules,
                      int original_size);

  void SetCriticalCssStats(
      int exception_count, int import_count, int link_count);

  // Mock to avoid dealing with property cache.
  virtual CriticalCssResult* GetCriticalCssFromCache(RewriteDriver* driver);

  virtual void ComputeCriticalCss(RewriteDriver* driver) {}

 private:
  RewriteDriver* driver_;
  scoped_ptr<CriticalCssResult> critical_css_result_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_CSS_FINDER_H_
