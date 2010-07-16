/**
 * Copyright 2010 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/util/public/data_url.h"

#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/content_type.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

void DataUrl(const ContentType& content_type,
                     const Encoding encoding,
                     const StringPiece& content,
                     std::string* result) {
  result->assign("data:");
  result->append(content_type.mime_type());
  switch (encoding) {
    case BASE64: {
      result->append(";base64,");
      std::string encoded;
      Mime64Encode(content, &encoded);
      result->append(encoded);
      break;
    }
//     case UTF8:
//       result->append(";charset=\"utf-8\",");
//       // TODO(jmaessen): find %-encoding code to use here.
//       //   jmarantz has one pending.
//       result.append(content);
//     case LATIN1:
//       result->append(";charset=\"\",");
//       // TODO(jmaessen): find %-encoding code to use here.
//       //   Not the UTF-8 one!
    case PLAIN: {
      // No special encoding or alphabet.
      result->append(",");
      content.AppendToString(result);
      break;
    }
  }
}

}  // namespace net_instaweb
