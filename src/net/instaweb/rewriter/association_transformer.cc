/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/rewriter/public/association_transformer.h"

#include <utility>

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

AssociationTransformer::~AssociationTransformer() {
}

CssTagScanner::Transformer::TransformStatus AssociationTransformer::Transform(
    GoogleString* str) {
  TransformStatus ret = kNoChange;

  // Note: we do not mess with empty URLs at all.
  if (!str->empty()) {
    GoogleUrl url(*base_url_, *str);
    if (!url.IsWebOrDataValid()) {
      handler_->Message(kInfo, "Invalid URL in CSS %s expands to %s",
                        str->c_str(), url.spec_c_str());
      ret = kFailure;
    } else {
      // Apply association.
      GoogleString url_string;
      url.Spec().CopyToString(&url_string);
      StringStringMap::const_iterator it = map_.find(url_string);
      if (it != map_.end()) {
        UrlRelativity url_relativity = GoogleUrl::FindRelativity(*str);
        *str = ResourceSlot::RelativizeOrPassthrough(
            options_, it->second, url_relativity, *base_url_);
        ret = kSuccess;
      } else if (backup_transformer_ != NULL) {
        ret = backup_transformer_->Transform(str);
      }
    }
  }

  return ret;
}

AssociationSlot::~AssociationSlot() {
}

}  // namespace net_instaweb
