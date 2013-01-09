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

#ifndef PAGESPEED_TIMELINE_JSON_IMPORTER_H_
#define PAGESPEED_TIMELINE_JSON_IMPORTER_H_

#include <string>
#include <vector>

class ListValue;

namespace pagespeed {

class InstrumentationData;

namespace timeline {

// Return false if there were any errors, true otherwise.
bool CreateTimelineProtoFromJsonString(
    const std::string& json_string,
    std::vector<const InstrumentationData*>* proto_out);

// Return false if there were any errors, true otherwise.
bool CreateTimelineProtoFromJsonValue(
    const ListValue& json,
    std::vector<const InstrumentationData*>* proto_out);

}  // namespace timeline

}  // namespace pagespeed

#endif  // PAGESPEED_TIMELINE_JSON_IMPORTER_H_
