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

#include "net/instaweb/rewriter/public/inline_output_resource.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/hasher.h"

namespace net_instaweb {

OutputResourcePtr InlineOutputResource::MakeInlineOutputResource(
    const RewriteDriver* driver) {
  ResourceNamer namer;
  return OutputResourcePtr(new InlineOutputResource(driver, namer));
}

InlineOutputResource::InlineOutputResource(const RewriteDriver* driver,
                                           const ResourceNamer& namer)
    : OutputResource(driver,
                     // TODO(sligocki): Modify OutputResource so that it does
                     // not depend upon having these dummy fields.
                     "dummy:/" /* resolved_base */,
                     "dummy:/" /* unmapped_base */,
                     "dummy:/" /* original_base */,
                     namer,
                     kInlineResource) {
}

GoogleString InlineOutputResource::url() const {
  LOG(DFATAL) << "Attempt to check inline resource URL.";
  return "";
}

GoogleString InlineOutputResource::UrlForDebug() const {
  // TODO(sligocki): It would be nice to be more specific, but we don't store
  // any information currently about where this resource is.
  return "Rewritten inline resource";
}

GoogleString InlineOutputResource::cache_key() const {
  CHECK(loaded());
  ResponseHeaders headers;
  const Hasher* hasher = server_context()->contents_hasher();
  return hasher->Hash(ExtractUncompressedContents());
}

}  // namespace net_instaweb
