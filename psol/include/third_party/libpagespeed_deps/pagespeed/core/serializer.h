// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_SERIALIZER_H_
#define PAGESPEED_CORE_SERIALIZER_H_

#include <string>

#include "base/basictypes.h"

namespace pagespeed {

/**
 * Serialize to file interface.
 */
class Serializer {
 public:
  Serializer() {}
  virtual ~Serializer() {}

  // Write content to a file
  //
  // @param content_url url from which content was originally served.
  // @param mime_type The MIME type of the body.
  // @param body Content body.
  // @return full file uri or an empty string if no file was written.
  virtual std::string SerializeToFile(const std::string& content_url,
                                      const std::string& mime_type,
                                      const std::string& body) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_SERIALIZER_H_
