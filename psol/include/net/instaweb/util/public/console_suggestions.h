// Copyright 2013 Google Inc.
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
//
// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CONSOLE_SUGGESTIONS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CONSOLE_SUGGESTIONS_H_

#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Statistics;

// One suggestion to go into the console. It contains an importance value
// (larger is more important) a message to tell the user the problem and
// a documentation URL (optionally empty if there is no doc).
// ConsoleSuggestions will be sorted by importance and the most important
// ones will be shown to the user in the PSOL console.
struct ConsoleSuggestion {
  ConsoleSuggestion(double importance_in, GoogleString message_in,
                    GoogleString doc_url_in)
      : importance(importance_in), message(message_in), doc_url(doc_url_in) {}

  double importance;
  GoogleString message;
  GoogleString doc_url;
};

// Class which gets all suggestions, sorts them and allows access to the
// results. Can be subclassed to add implementation-specific suggestions.
class ConsoleSuggestionsFactory {
 public:
  explicit ConsoleSuggestionsFactory(Statistics* stats) : stats_(stats) {}
  virtual ~ConsoleSuggestionsFactory();

  // Generate suggestions_.
  // If subclass wants to add it's own suggestions, it should do that first
  // and then call ConsoleSuggestionsFactory::GenerateSuggestions().
  virtual void GenerateSuggestions();

  // Returns the importance-sorted list of ConsoleSuggestions after
  // GenerateSuggestions() has been called.
  const std::vector<ConsoleSuggestion>* suggestions() const {
    return &suggestions_;
  }

 protected:
  // Statistics helper functions.
  // Gets value of specific variable (DFATALs if variable doesn't exist).
  int64 StatValue(StringPiece var_name);
  // Returns ratio of variables (or 0 if denominator is 0).
  double StatRatio(StringPiece numerator, StringPiece denominator);
  // Returns ratio of bad / (good + bad). Common pattern for our stats.
  // (Returns 0 if good + bad = 0).
  double StatSumRatio(StringPiece bad, StringPiece good);

 private:
  FRIEND_TEST(ConsoleSuggestionsTest, Stats);

  Statistics* stats_;
  std::vector<ConsoleSuggestion> suggestions_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleSuggestionsFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CONSOLE_SUGGESTIONS_H_
