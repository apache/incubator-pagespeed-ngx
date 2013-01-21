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

#ifndef PAGESPEED_APPS_PAGESPEED_OUTPUT_UTIL_H_
#define PAGESPEED_APPS_PAGESPEED_OUTPUT_UTIL_H_

namespace pagespeed {

class Results;

namespace proto {

// Returns true if all Result instances in the Results object have ids
// assigned, false otherwise.
bool AllResultsHaveIds(const pagespeed::Results& results);

// Removes ids from all Result instances in the Results object.
void ClearResultIds(pagespeed::Results* results);

// Populates each Results instance with a unique and stable ID,
// assuming that the structure of the protocol buffer never changes.
// If one or more result instances already has an ID assigned, does
// nothing and returns false. Otherwise, assigns IDs and returns true.
bool PopulateResultIds(pagespeed::Results* results);

}  // namespace proto

}  // namespace pagespeed

#endif  // PAGESPEED_APPS_PAGESPEED_OUTPUT_UTIL_H_
