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

// Author: jefftk@google.com (Jeff Kaufman)
//
// Implementations of FileLoadRuleLiteral and FileLoadRuleRegexp, two
// subclasses of the abstract class FileLoadRule, in addition to implementation
// of FileLoadRule.
//
// Tests are in file_load_policy_test.

#include "net/instaweb/rewriter/public/file_load_rule.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

FileLoadRule::Classification FileLoadRule::Classify(
    const GoogleString& filename) const {
  if (Match(filename)) {
    if (allowed_) {
      return kAllowed;
    } else {
      return kDisallowed;
    }
  } else {
    return kUnmatched;
  }
}

FileLoadRule::~FileLoadRule() {}
FileLoadRuleRegexp::~FileLoadRuleRegexp() {}
FileLoadRuleLiteral::~FileLoadRuleLiteral() {}

FileLoadRule* FileLoadRuleRegexp::Clone() const {
  // TODO(jefftk): Convert to reference counting the RE2.  Se TODO in
  // FileLoadMappingRegexp.
  return new FileLoadRuleRegexp(filename_regexp_str_, allowed_);
}

bool FileLoadRuleRegexp::Match(const GoogleString& filename) const {
  return RE2::PartialMatch(filename, filename_regexp_str_);
}

FileLoadRule* FileLoadRuleLiteral::Clone() const {
  return new FileLoadRuleLiteral(filename_prefix_, allowed_);
}

bool FileLoadRuleLiteral::Match(const GoogleString& filename) const {
  return StringPiece(filename).starts_with(filename_prefix_);
}

}  // namespace net_instaweb
