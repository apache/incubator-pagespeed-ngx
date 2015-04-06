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

#include "net/instaweb/rewriter/public/mock_critical_css_finder.h"

#include "net/instaweb/rewriter/critical_css.pb.h"

namespace net_instaweb {

MockCriticalCssFinder::~MockCriticalCssFinder() {}

void MockCriticalCssFinder::AddCriticalCss(const StringPiece& url,
                                           const StringPiece& rules,
                                           int original_size) {
  if (critical_css_result_.get() == NULL) {
    critical_css_result_.reset(new CriticalCssResult());
  }
  CriticalCssResult_LinkRules* link_rules =
      critical_css_result_->add_link_rules();
  link_rules->set_link_url(url.as_string());
  link_rules->set_critical_rules(rules.as_string());
  link_rules->set_original_size(original_size);
}

void MockCriticalCssFinder::SetCriticalCssStats(
    int exception_count, int import_count, int link_count) {
  if (critical_css_result_.get() == NULL) {
    critical_css_result_.reset(new CriticalCssResult());
  }
  critical_css_result_->set_exception_count(exception_count);
  critical_css_result_->set_import_count(import_count);
  critical_css_result_->set_link_count(link_count);
}

CriticalCssResult* MockCriticalCssFinder::GetCriticalCssFromCache(
    RewriteDriver* driver) {
  CriticalCssResult* result = critical_css_result_.release();

  if (result != NULL) {
    // Add back the rules so the second driver can find it also.
    critical_css_result_.reset(new CriticalCssResult(*result));
  }
  return result;
}

}  // namespace net_instaweb
