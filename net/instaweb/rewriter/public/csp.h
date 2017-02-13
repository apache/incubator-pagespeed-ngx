/*
 * Copyright 2017 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This provides basic parsing and evaluation of a (subset of)
// Content-Security-Policy that's relevant for PageSpeed Automatic.
// CspContext is the main class.

#ifndef NET_INSTAWEB_REWRITER_CSP_H_
#define NET_INSTAWEB_REWRITER_CSP_H_

#include <memory>
#include <string>
#include <vector>

#include "net/instaweb/rewriter/public/csp_directive.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class CspSourceExpression {
 public:
  enum Kind {
    kSelf, kSchemeSource, kHostSource,
    kUnsafeInline, kUnsafeEval, kStrictDynamic, kUnsafeHashedAttributes,
    kUnknown /* includes hash-or-nonce */
  };

  CspSourceExpression() : kind_(kUnknown) {}
  explicit CspSourceExpression(Kind kind): kind_(kind) {}
  CspSourceExpression(Kind kind, StringPiece input)
      : kind_(kind), param_(input.as_string()) {}

  static CspSourceExpression Parse(StringPiece input);

  bool operator==(const CspSourceExpression& other) const {
    return kind_ == other.kind_ && param_ == other.param_;
  }

  Kind kind() const { return kind_; }
  const GoogleString& param() const { return param_; }

 private:
  // input here is without the quotes, and non-empty.
  static CspSourceExpression ParseQuoted(StringPiece input);

  Kind kind_;
  GoogleString param_;
};

class CspSourceList {
 public:
  static std::unique_ptr<CspSourceList> Parse(StringPiece input);
  const std::vector<CspSourceExpression>& expressions() const {
    return expressions_;
  }

 private:
  std::vector<CspSourceExpression> expressions_;
};

// An individual policy. Note that a page is constrained by an intersection
// of some number of these.
class CspPolicy {
 public:
  CspPolicy();

  // Just an example for now...
  bool UnsafeEval() const { return false; /* */ }

  // May return null.
  static std::unique_ptr<CspPolicy> Parse(StringPiece input);

  // May return null.
  const CspSourceList* SourceListFor(CspDirective directive) {
    return policies_[static_cast<int>(directive)].get();
  }

 private:
  // The expectation is that some of these may be null.
  std::vector<std::unique_ptr<CspSourceList>> policies_;
};

// A set of all policies (maybe none!) on the page. Note that we do not track
// those with report disposition, only those that actually enforce --- reporting
// seems like it would keep the page author informed about our effects as it is.
class CspContext {
 public:
  bool UnsafeEval() const {
    return AllPermit(&CspPolicy::UnsafeEval);
  }

 private:
  typedef bool (CspPolicy::*SimplePredicateFn)() const;

  bool AllPermit(SimplePredicateFn predicate) const {
    // Note that empty policies_ means "true" --- there is no policy whatsoever,
    // so everything is permitted. If there is more than that, all policies
    // must agree, too.
    for (const auto& policy : policies_) {
      if (!(policy.get()->*predicate)()) {
        return false;
      }
    }
    return true;
  }

  std::vector<std::unique_ptr<CspPolicy>> policies_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_CSP_H_
