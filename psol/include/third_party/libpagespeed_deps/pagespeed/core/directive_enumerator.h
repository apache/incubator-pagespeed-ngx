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

#ifndef PAGESPEED_CORE_DIRECTIVE_ENUMERATOR_H_
#define PAGESPEED_CORE_DIRECTIVE_ENUMERATOR_H_

#include <string>
#include "base/basictypes.h"
#include "pagespeed/core/string_tokenizer.h"

namespace pagespeed {

// Enumerates HTTP header directives.  For instance, given
// Cache-Control: private, no-store, max-age=100, the
// DirectiveEnumerator will return each Cache-Control directive as a
// k,v pair:
//
// private,NULL
// no-store,NULL
// max-age,100
class DirectiveEnumerator {
 public:
  explicit DirectiveEnumerator(const std::string& header);

  bool GetNext(std::string* key, std::string* value);

  bool done() const { return state_ == STATE_DONE; }
  bool error() const { return state_ == STATE_ERROR; }

 private:
  enum State {
    STATE_START,
    CONSUMED_KEY,
    CONSUMED_EQ,
    CONSUMED_VALUE,
    STATE_DONE,
    STATE_ERROR,
  };

  bool CanTransition(State src, State dest) const;
  bool Transition(State dest);

  bool GetNextInternal(std::string* key, std::string* value);
  bool OnDelimiter(char c);
  bool OnToken(std::string* key, std::string* value);

  std::string header_;
  StringTokenizer tok_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveEnumerator);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_DIRECTIVE_ENUMERATOR_H_
