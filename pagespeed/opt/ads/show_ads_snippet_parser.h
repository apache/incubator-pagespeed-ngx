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
// Author: chenyu@google.com (Yu Chen)

#ifndef PAGESPEED_KERNEL_ADS_ADS_SNIPPET_PARSER_H_
#define PAGESPEED_KERNEL_ADS_ADS_SNIPPET_PARSER_H_

#include <map>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

namespace net_instaweb {

// Class that parses showads snippets.
class ShowAdsSnippetParser {
 public:
  typedef std::map<GoogleString, GoogleString> AttributeMap;

  ShowAdsSnippetParser() {}
  ~ShowAdsSnippetParser() {}

  // Parses showads attributes from 'snippet' and stores the parsed attributes
  // in 'parsed_attributes'. It returns true if snippet contains
  // only assignments of showads attributes and there is at most one assignment
  // for each showads attribute. The data in 'parsed_attributes' is meaningful
  // only when this method returns true.
  bool ParseStrict(const GoogleString& snippet,
                   const pagespeed::js::JsTokenizerPatterns* tokenizer_patterns,
                   AttributeMap* parsed_attributes) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowAdsSnippetParser);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_ADS_ADS_SNIPPET_PARSER_H_
