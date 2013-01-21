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

#ifndef PAGESPEED_CORE_HTTP_ARCHIVE_H_
#define PAGESPEED_CORE_HTTP_ARCHIVE_H_

#include <string>

#include "base/basictypes.h"  // for int64

namespace pagespeed {

class PagespeedInput;
class ResourceFilter;

// Parse the HAR string into a PagespeedInput, or return NULL if there is an
// error.
PagespeedInput* ParseHttpArchive(const std::string& har_data);

// Parse the HAR string into a PagespeedInput using the given resource filter,
// or return NULL if there is an error.  The returned PagespeedInput will take
// ownership of the ResourceFilter object; if this function returns NULL, it
// will delete the ResourceFilter before returning.
PagespeedInput* ParseHttpArchiveWithFilter(const std::string& har_data,
                                           ResourceFilter* resource_filter);

// Given a string in ISO 8601 format (see http://www.w3.org/TR/NOTE-datetime),
// produce the number of milliseconds from midnight on 1 Jan 1970 UTC.  If the
// parse is successful, return true; otherwise return false and make no change
// to &output.
bool Iso8601ToEpochMillis(const std::string& input, int64* output);

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_HTTP_ARCHIVE_H_
