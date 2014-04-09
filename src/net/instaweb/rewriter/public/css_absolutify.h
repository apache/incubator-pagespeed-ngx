/*
 * Copyright 2014 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_ABSOLUTIFY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_ABSOLUTIFY_H_

#include "net/instaweb/util/public/basictypes.h"

namespace Css {
class Stylesheet;
}  // namespace Css

namespace net_instaweb {

class GoogleUrl;
class RewriteDriver;
class MessageHandler;

class CssAbsolutify {
 public:
  // Absolutify all relative URLs in the stylesheet's imports using the given
  // base URL. The Import structures are modified in-situ. Returns true if any
  // URLs were absolutified, false if not.
  static bool AbsolutifyImports(Css::Stylesheet* stylesheet,
                                const GoogleUrl& base);

  // Absolutify all relative URLs in the stylesheet using the given base URL.
  // The Declaration structures are modified in-situ. You can control whether
  // URLs in parseable sections (BACKGROUND, BACKGROUND_IMAGE, LIST_STYLE,
  // LIST_STYLE_IMAGE) and/or unparseable sections (UNPARSEABLE) are handled.
  // Returns true if any URLs were absolutified, false if not.
  static bool AbsolutifyUrls(Css::Stylesheet* stylesheet,
                             const GoogleUrl& base,
                             bool handle_parseable_sections,
                             bool handle_unparseable_sections,
                             RewriteDriver* driver,
                             MessageHandler* handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(CssAbsolutify);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_ABSOLUTIFY_H_
