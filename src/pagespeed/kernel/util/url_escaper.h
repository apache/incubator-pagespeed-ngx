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

#ifndef PAGESPEED_KERNEL_UTIL_URL_ESCAPER_H_
#define PAGESPEED_KERNEL_UTIL_URL_ESCAPER_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

/*
The intent of this class to encode arbitrary URLs into one 'segment'
of a new URL. We escape many special chars like /\?&% to make sure that
this encoded version is only one segment.

We would like the above URL to be reasonably legible if possible.
However it's also nice if it's short.

Common URL format is:

  http://www.foo.bar/z1234/b_c.d?e=f&g=h

this suggests we should have short encodings for  a-zA-Z0-9./?&

The transform table is:

a-zA-Z0-9_=+-. unchanged
^               ,u
%               ,P
/               ,_
\               ,-
,               ,,
?               ,q
&               ,a
http://         ,h
.pagespeed.     ,M

everything else ,XX  where XX are hex digits using capital letters.
*/

namespace UrlEscaper {

void EncodeToUrlSegment(const StringPiece& in, GoogleString* url_segment);
bool DecodeFromUrlSegment(const StringPiece& url_segment, GoogleString* out);

}  // namespace UrlEscaper

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_URL_ESCAPER_H_
