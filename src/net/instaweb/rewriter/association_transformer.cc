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

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

AssociationTransformer::~AssociationTransformer() {
}

CssTagScanner::Transformer::TransformStatus AssociationTransformer::Transform(
    const StringPiece& in, GoogleString* out) {
  TransformStatus ret = kNoChange;

  // Note: we do not mess with empty URLs at all.
  if (!in.empty()) {
    GoogleUrl in_url(*base_url_, in);
    if (!in_url.is_valid()) {
      handler_->Message(kInfo, "Invalid URL in CSS %s expands to %s",
                        in.as_string().c_str(), in_url.spec_c_str());
      ret = kFailure;
    } else {
      // Apply association.
      GoogleString in_string;
      in_url.Spec().CopyToString(&in_string);
      StringStringMap::const_iterator it = map_.find(in_string);
      if (it != map_.end()) {
        *out = it->second;
        ret = kSuccess;
      } else {
        if (backup_transformer_ != NULL) {
          ret = backup_transformer_->Transform(in, out);
        } else {
          // If backup_transformer_ is not set, just copy in->out directly.
          in.CopyToString(out);
        }
      }
    }
  }

  return ret;
}

AssociationSlot::~AssociationSlot() {
}

}  // namespace net_instaweb
