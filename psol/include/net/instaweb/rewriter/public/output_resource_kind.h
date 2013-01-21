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

// Author: jmaessen@google.com (Jan-Willem Maessen)
//
// Declare OutputResourceKind enum, to avoid a circular include loop.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_KIND_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_KIND_H_

namespace net_instaweb {

enum OutputResourceKind {
  kRewrittenResource,  // derived from some input resource URL or URLs.
  kOnTheFlyResource,   // derived from some input resource URL or URLs in a
                       //   very inexpensive way --- it makes no sense to
                       //   cache the output contents.
  kOutlinedResource    // derived from page HTML.
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_KIND_H_
