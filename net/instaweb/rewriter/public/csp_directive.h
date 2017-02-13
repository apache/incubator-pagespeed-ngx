/*
 * Copyright 2017 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Enum for Content-Security-Policy directives

#ifndef NET_INSTAWEB_REWRITER_CSP_DIRECTIVE_H_
#define NET_INSTAWEB_REWRITER_CSP_DIRECTIVE_H_

#include <memory>
#include <string>
#include <vector>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct CspDirectiveInfo;

// Directives mentioned in the spec that we care (and comments for those
// where we don't).
enum class CspDirective {
  // These take source list:
  kChildSrc,
  kConnectSrc,
  kDefaultSrc,
  // font-src doesn't actually matter for us since the Google font support only
  // touches the loader CSS, not the font URL itself.
  kFrameSrc,
  kImgSrc,
  // manifest-src
  // media-src
  // object-src
  kScriptSrc,
  kStyleSrc,
  // worker-src
  kBaseUri,
  // form-action
  // frame-ancestors

  kNumSourceListDirectives

  // These take other stuff. If we actually parsed them, we would want
  // to distinguish them so we don't stick them into the array of
  // CspSourceList* the other stuff goes into.

  // plugin-types
  // sandbox --- TODO(morlovich): Understand implications of this.
  // disown-opener
  // report-uri
  // report-to
};

// Returns kNumSourceListDirectives if unrecognized.
CspDirective LookupCspDirective(StringPiece name);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_CSP_DIRECTIVE_H_
