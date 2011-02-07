/*
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ESCAPE_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ESCAPE_H_

#include "net/instaweb/htmlparse/public/html_keywords.h"

namespace net_instaweb {

// TODO(jmarantz): Remove compatibility class for Page Speed, which
// cannot easily be updated to rename its references HtmlEscape to
// HtmlKeywords in one CL.
class HtmlEscape {
 public:
  static void Init() { HtmlKeywords::Init(); }
  static void ShutDown() { HtmlKeywords::ShutDown(); }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ESCAPE_H_
