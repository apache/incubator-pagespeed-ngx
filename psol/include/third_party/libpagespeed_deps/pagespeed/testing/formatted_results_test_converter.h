// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_TESTING_FORMATTED_RESULTS_TEST_CONVERTER_H_
#define PAGESPEED_TESTING_FORMATTED_RESULTS_TEST_CONVERTER_H_

#include <string>

#include "base/basictypes.h"

namespace pagespeed {

class FormatArgument;
class FormatString;
class FormattedResults;
class FormattedRuleResults;
class FormattedUrlResult;
class FormattedUrlBlockResults;

}  // namespace pagespeed

namespace pagespeed_testing {

/**
 * Converts a Results protobuf to text.
 */
class FormattedResultsTestConverter {
 public:
  // Converts a FormattedResults protocol buffer to a text string.  Will return
  // false on failure.
  static bool Convert(const pagespeed::FormattedResults& results,
                      std::string* out);

  // Converts the various protocol buffers in a FormattedResults
  // structure into text.
  static bool ConvertFormattedResults(
      const pagespeed::FormattedResults& results, std::string* out);
  static bool ConvertFormattedRuleResults(
      const pagespeed::FormattedRuleResults& rule_results, std::string* out);
  static bool ConvertFormattedUrlBlockResults(
      const pagespeed::FormattedUrlBlockResults& url_block_results,
      std::string* out);
  static bool ConvertFormattedUrlResult(
      const pagespeed::FormattedUrlResult& url_result, std::string* out);
  static void ConvertFormatString(
      const pagespeed::FormatString& format_string, std::string* out);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FormattedResultsTestConverter);
};

}  // namespace pagespeed_testing

#endif  // PAGESPEED_PROTO_FORMATTED_RESULTS_TO_TEXT_CONVERTER_H_
