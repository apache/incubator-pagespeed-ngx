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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const ContentType kTypes[] = {
  {"text/javascript", ".js"},
  {"text/css", ".css"},
  {"text/plain", ".txt"},

  {"image/png", ".png"},
  {"image/gif", ".gif"},
  {"image/jpeg", ".jpg"},
};
const int kNumTypes = arraysize(kTypes);

}  // namespace

const ContentType& kContentTypeJavascript = kTypes[0];
const ContentType& kContentTypeCss = kTypes[1];
const ContentType& kContentTypeText = kTypes[2];

const ContentType& kContentTypePng = kTypes[3];
const ContentType& kContentTypeGif = kTypes[4];
const ContentType& kContentTypeJpeg = kTypes[5];

const ContentType* NameExtensionToContentType(const StringPiece& name) {
  // TODO(jmarantz): convert to a map if the list gets large.
  const ContentType* res = NULL;
  for (int i = 0; i < kNumTypes; ++i) {
    if (name.ends_with(kTypes[i].file_extension())) {
      res = &kTypes[i];
      break;
    }
  }
  return res;
}

}  // namespace net_instaweb
