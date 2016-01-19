/*
 * Copyright 2014 Google Inc.
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

// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_KERNEL_HTML_DISABLED_TEST_FILTER_H_
#define PAGESPEED_KERNEL_HTML_DISABLED_TEST_FILTER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class DisableTestFilter : public EmptyHtmlFilter {
 public:
  DisableTestFilter(const GoogleString& name, bool is_enabled,
                    const GoogleString& disabled_reason)
      : name_(name),
        is_enabled_(is_enabled),
        disabled_reason_(disabled_reason) {}

  virtual void DetermineEnabled(GoogleString* disabled_reason) {
    set_is_enabled(is_enabled_);
    // Note that disabled_reason is always set, even if is_enabled is false.
    // This allows us to verify that it will be ignored when is_enabled is
    // true.
    *disabled_reason = disabled_reason_;
  }

  GoogleString ExpectedDisabledMessage() const {
    GoogleString message(Name());
    if (!disabled_reason_.empty()) {
      StrAppend(&message, ": ", disabled_reason_);
    }
    return message;
  }

  virtual const char* Name() const { return name_.c_str(); }

 private:
  GoogleString name_;
  bool is_enabled_;
  GoogleString disabled_reason_;

  DISALLOW_COPY_AND_ASSIGN(DisableTestFilter);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_DISABLED_TEST_FILTER_H_
