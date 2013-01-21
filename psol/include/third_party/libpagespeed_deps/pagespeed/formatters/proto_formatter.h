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

// Author: aoates@google.com (Andrew Oates)

#ifndef PAGESPEED_FORMATTERS_PROTO_FORMATTER_H_
#define PAGESPEED_FORMATTERS_PROTO_FORMATTER_H_

#include <vector>

#include "base/basictypes.h"
#include "pagespeed/core/formatter.h"
#include "pagespeed/core/serializer.h"

namespace pagespeed {

class FormatArgument;
class FormattedResults;
class FormattedRuleResults;
class FormattedUrlBlockResults;
class FormattedUrlResult;
namespace l10n { class Localizer; }

namespace formatters {

/**
 * Formatter that fills in a localized FormattedResults proto.
 */
class ProtoFormatter : public Formatter {
 public:
  ProtoFormatter(const pagespeed::l10n::Localizer* localizer,
                 FormattedResults* results);
  ~ProtoFormatter();

  // Formatter interface.
  virtual RuleFormatter* AddRule(const Rule& rule, int score, double impact);
  void SetOverallScore(int score);
  void Finalize();

 private:
  const pagespeed::l10n::Localizer* localizer_;
  FormattedResults* results_;
  std::vector<RuleFormatter*> rule_formatters_;

  DISALLOW_COPY_AND_ASSIGN(ProtoFormatter);
};

class ProtoRuleFormatter : public RuleFormatter {
 public:
  ProtoRuleFormatter(const pagespeed::l10n::Localizer* localizer,
                     FormattedRuleResults* rule_results);
  ~ProtoRuleFormatter();

  // RuleFormatter interface.
  virtual UrlBlockFormatter* AddUrlBlock(
      const UserFacingString& format_str,
      const std::vector<const FormatArgument*>& arguments);

 private:
  const pagespeed::l10n::Localizer* localizer_;
  FormattedRuleResults* rule_results_;
  std::vector<UrlBlockFormatter*> url_block_formatters_;

  DISALLOW_COPY_AND_ASSIGN(ProtoRuleFormatter);
};

class ProtoUrlBlockFormatter : public UrlBlockFormatter {
 public:
  ProtoUrlBlockFormatter(const pagespeed::l10n::Localizer* localizer,
                         FormattedUrlBlockResults* url_block_results);
  ~ProtoUrlBlockFormatter();

  // UrlBlockFormatter interface.
  virtual UrlFormatter* AddUrlResult(
      const UserFacingString& format_str,
      const std::vector<const FormatArgument*>& arguments);

 private:
  const pagespeed::l10n::Localizer* localizer_;
  FormattedUrlBlockResults* url_block_results_;
  std::vector<UrlFormatter*> url_formatters_;

  DISALLOW_COPY_AND_ASSIGN(ProtoUrlBlockFormatter);
};

class ProtoUrlFormatter : public UrlFormatter {
 public:
  ProtoUrlFormatter(const pagespeed::l10n::Localizer* localizer,
                    FormattedUrlResult* url_result);

  // UrlFormatter interface.
  virtual void AddDetail(
      const UserFacingString& format_str,
      const std::vector<const FormatArgument*>& arguments);
  virtual void SetAssociatedResultId(int id);

 private:
  const pagespeed::l10n::Localizer* localizer_;
  FormattedUrlResult* url_result_;

  DISALLOW_COPY_AND_ASSIGN(ProtoUrlFormatter);
};

}  // namespace formatters

}  // namespace pagespeed

#endif  // PAGESPEED_FORMATTERS_PROTO_FORMATTER_H_
