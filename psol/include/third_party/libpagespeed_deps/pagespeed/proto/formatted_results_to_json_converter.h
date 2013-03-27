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

#ifndef PAGESPEED_PROTO_FORMATTED_RESULTS_TO_JSON_CONVERTER_H_
#define PAGESPEED_PROTO_FORMATTED_RESULTS_TO_JSON_CONVERTER_H_

#include <string>

#include "base/basictypes.h"

namespace base {
class Value;
}  // namespace base

namespace pagespeed {

class FormatArgument;
class FormatString;
class FormattedResults;
class FormattedRuleResults;
class FormattedUrlResult;
class FormattedUrlBlockResults;

namespace proto {

/**
 * Converts a Results protobuf to JSON.
 */
class FormattedResultsToJsonConverter {
 public:
  // Converts a FormattedResults protocol buffer to a JSON
  // string. Will return false on failure.
  static bool Convert(const pagespeed::FormattedResults& results,
                      std::string* out);

  // Converts the various protocol buffers in a FormattedResults
  // structure into a base::Value* object, whsich can be converted to
  // JSON with a JSONWriter. Ownership of the returned base::Value* is
  // transferred to the caller. Will return NULL if conversion fails.
  static base::Value* ConvertFormattedResults(
      const pagespeed::FormattedResults& results);
  static base::Value* ConvertFormattedRuleResults(
      const pagespeed::FormattedRuleResults& rule_results);
  static base::Value* ConvertFormattedUrlBlockResults(
      const pagespeed::FormattedUrlBlockResults& url_block_results);
  static base::Value* ConvertFormattedUrlResult(
      const pagespeed::FormattedUrlResult& url_result);
  static base::Value* ConvertFormatString(
      const pagespeed::FormatString& format_string);
  static base::Value* ConvertFormatArgument(
      const pagespeed::FormatArgument& format_arg);
  static const char* ConvertFormatArgumentType(int argument_type);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FormattedResultsToJsonConverter);
};

}  // namespace proto

}  // namespace pagespeed

#endif  // PAGESPEED_PROTO_FORMATTED_RESULTS_TO_JSON_CONVERTER_H_
