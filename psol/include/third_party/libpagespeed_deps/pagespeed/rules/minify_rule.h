// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_RULES_MINIFY_RULE_H_
#define PAGESPEED_RULES_MINIFY_RULE_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

// Compute the rule score as a function of the "cost" of the rule,
// where the cost is usually the number of wasted bytes.
class CostBasedScoreComputer {
 public:
  CostBasedScoreComputer(int64 max_possible_cost);
  virtual ~CostBasedScoreComputer();

  int ComputeScore();

 protected:
  virtual int64 ComputeCost() = 0;

  const int64 max_possible_cost_;
};

// Compute a rule score as a function of the "cost" of the rule,
// taking a cost weight into account.  For many minification rules,
// there is no upper bound on how large an unoptimized resource can
// be, and thus no limit to the possible cost. Each of these rules
// specifies a "cost weight" multiplier that maps the cost into a
// range that distributes scores into a reasonable distribution from
// 0..100.  The weights were chosen by analyzing the resources of the top
// 100 web sites.
class WeightedCostBasedScoreComputer : public CostBasedScoreComputer {
 public:
  WeightedCostBasedScoreComputer(const RuleResults* results,
                                 int64 max_possible_cost,
                                 double cost_weight);

 protected:
  virtual int64 ComputeCost();

 private:
  const RuleResults* const results_;
  const double cost_weight_;
};

struct MinifierOutput {
 public:
  // Indicate an error in the rule.
  static MinifierOutput* Error() { return NULL; }

  // No error, but this resource is not eligible for minification by this rule.
  static MinifierOutput* CannotBeMinified();

  // Provide the minified size, but not the minified content.  This is only
  // valid for resources that were _not_ served compressed.
  static MinifierOutput* PlainMinifiedSize(int plain_minified_size);

  // Successfully minified content, but should not be saved to disk.
  static MinifierOutput* DoNotSaveMinifiedContent(
      const std::string& minified_content);

  // Minified content, to be saved to disk (assuming the savings is positive).
  // The minified_content_mime_type argument must be non-empty.
  static MinifierOutput* SaveMinifiedContent(
      const std::string& minified_content,
      const std::string& minified_content_mime_type);

  // False if the resource was not eligible for minification (even if this
  // returns true, however, the savings may be non-positive).
  bool can_be_minified() const { return can_be_minified_; }

  // The size of the resource after minification, without additional
  // compression.
  int plain_minified_size() const { return plain_minified_size_; }

  // True if the minified content should be saved.
  bool should_save_minified_content() const {
    return !minified_content_mime_type_.empty();
  }

  // The minified content; this is only guaranteed to be non-NULL if
  // should_save_minified_content() returns true.
  const std::string* minified_content() const {
    return minified_content_.get();
  }

  // The MIME type of the minified content (possibly different than the MIME
  // type of the original resource).  This is guaranteed to be non-empty only
  // if should_save_minified_content() returns true.
  const std::string& minified_content_mime_type() const {
    return minified_content_mime_type_;
  }

  // Get the size of the minified resource after also being compressed.  Return
  // true on success, false on failure.
  bool GetCompressedMinifiedSize(int* output) const;

 private:
  MinifierOutput(bool can_be_minified,
                 int plain_minified_size,
                 const std::string* minified_content,
                 const std::string& minified_content_mime_type);

  const bool can_be_minified_;
  const int plain_minified_size_;
  scoped_ptr<const std::string> minified_content_;
  const std::string minified_content_mime_type_;

  DISALLOW_COPY_AND_ASSIGN(MinifierOutput);
};

class Minifier {
 public:
  Minifier();
  virtual ~Minifier();

  virtual const char* name() const = 0;
  virtual UserFacingString header_format() const = 0;
  virtual UserFacingString body_format() const = 0;
  virtual UserFacingString child_format() const = 0;
  virtual UserFacingString child_format_post_gzip() const = 0;
  virtual const MinifierOutput* Minify(const Resource& resource,
                                       const RuleInput& input) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Minifier);
};

/**
 * Class for rules that reduce the size of resources.
 */
class MinifyRule : public Rule {
 public:
  explicit MinifyRule(Minifier* minifier);
  virtual ~MinifyRule();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
 private:
  scoped_ptr<Minifier> minifier_;

  DISALLOW_COPY_AND_ASSIGN(MinifyRule);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_MINIFY_RULE_H_
