/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_WEBP_OPTIMIZER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_WEBP_OPTIMIZER_H_

#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Optimizer in the style of pagespeed/image_compression/jpeg_optimizer.h that
// creates a webp-formatted image in compressed_webp from the jpeg image in
// original_jpeg.  Indicates failure by returning false, in which case
// compressed_webp may be filled with junk.
bool OptimizeWebp(const GoogleString& original_jpeg,
                  GoogleString* compressed_webp);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_WEBP_OPTIMIZER_H_
