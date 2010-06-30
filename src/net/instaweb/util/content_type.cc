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

namespace net_instaweb {

const ContentType kContentTypeJavascript = {"text/javascript", ".js"};
const ContentType kContentTypeCss = {"text/css", ".css"};
const ContentType kContentTypeText = {"text/plain", ".txt"};

const ContentType kContentTypePng = {"image/png", ".png"};
const ContentType kContentTypeGif = {"image/gif", ".gif"};
const ContentType kContentTypeJpeg = {"image/jpeg", ".jpg"};

}  // namespace net_instaweb
