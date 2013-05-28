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

#include "net/instaweb/system/public/system_console_suggestions.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/file_cache.h"

namespace net_instaweb {

SystemConsoleSuggestionsFactory::~SystemConsoleSuggestionsFactory() {
}

void SystemConsoleSuggestionsFactory::GenerateSuggestions() {
  // Cannot fetch resources.
  // TODO(sligocki): Move Serf to this directory so we can ref these strings
  // as constants.
  // TODO(sligocki): Perhaps have a generic fetch_failure_count stat for all
  // fetcher? Although it may be better to measure each separately in the
  // console because they could each have distinct failure modes.
  AddConsoleSuggestion(StatRatio("serf_fetch_failure_count",
                                 "serf_fetch_request_count"),
                       "Resources not loaded because of fetch failure: %.2f%%",
                       // TODO(sligocki): Add doc links.
                       "");

  ConsoleSuggestionsFactory::GenerateSuggestions();
}

}  // namespace net_instaweb
