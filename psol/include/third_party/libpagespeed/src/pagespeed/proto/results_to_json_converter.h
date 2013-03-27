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

#ifndef PAGESPEED_PROTO_RESULTS_TO_JSON_CONVERTER_H_
#define PAGESPEED_PROTO_RESULTS_TO_JSON_CONVERTER_H_

#include <string>

#include "base/basictypes.h"

namespace base {
class Value;
}  // namespace base

namespace pagespeed {

class Result;
class Results;
class RuleResults;
class Savings;
class Version;

namespace proto {

/**
 * Converts a Results protobuf to JSON.
 */
class ResultsToJsonConverter {
 public:
  // Converts a Results protocol buffer to a JSON string. Will return
  // false on failure.
  static bool Convert(const pagespeed::Results& results, std::string* out);

  // Converts the various protocol buffers in a Results structure into
  // a base::Value* object, which can be converted to JSON with a
  // JSONWriter. Ownership of the returned base::Value* is transferred
  // to the caller. Will return NULL if conversion fails.
  static base::Value* ConvertResults(const pagespeed::Results& results);
  static base::Value* ConvertRuleResult(
      const pagespeed::RuleResults& rule_results);
  static base::Value* ConvertResult(const pagespeed::Result& result);
  static base::Value* ConvertSavings(const pagespeed::Savings& savings);
  static base::Value* ConvertVersion(const pagespeed::Version& version);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ResultsToJsonConverter);
};

}  // namespace proto

}  // namespace pagespeed

#endif  // PAGESPEED_PROTO_RESULTS_TO_JSON_CONVERTER_H_
