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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ASSOCIATION_TRANSFORMER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ASSOCIATION_TRANSFORMER_H_

#include <map>

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

class GoogleUrl;
class MessageHandler;

// Transformer that uses a std::map to specify which URLs to rewrite to
// which other URLs.
// Used by CssFilter to rewrite subresources in CSS even when it cannot
// be parsed, by using AssociationSlots to update the map before transforming.
class AssociationTransformer : public CssTagScanner::Transformer {
 public:
  // base_url is the URL all CSS url()s should be absolutified against,
  // this is generally the URL for the CSS file or HTML file for inline CSS.
  // backup_transformer is another transformer to be applied if no
  // association has been set in AssociationTransformer's map_. It may be
  // set to NULL if no backup is needed.
  //
  // base_url, backup_transformer and handler must live longer than
  // AssociationTransformer.
  AssociationTransformer(const GoogleUrl* base_url,
                         CssTagScanner::Transformer* backup_transformer,
                         MessageHandler* handler)
      : base_url_(base_url), backup_transformer_(backup_transformer),
        handler_(handler) {}
  virtual ~AssociationTransformer();

  // Map is exposed so that you can set associations.
  // Each key -> value specifies that every instance of the absolute URL
  // key should be transformed to the absolute URL value.
  StringStringMap* map() { return &map_; }

  // To do the actual transformation. Call CssTagScanner::TransformUrls()
  // with this AssociationTransformer which will call Transform() on all URLs.
  // Transform will lookup all (absolutified) URLs in map_ and rewrite them
  // if present (otherwise it will pass them to the backup_transformer_).
  virtual TransformStatus Transform(const StringPiece& in, GoogleString* out);

 private:
  // Mapping of input URLs to output URLs.
  StringStringMap map_;

  // Base URL for CSS file, needed to absolutify URLs in Transform.
  const GoogleUrl* base_url_;

  // Transformer to be applied to URLs we don't rewrite. For example, we might
  // want to make sure we absolutify all URLs, even if we don't rewrite them.
  CssTagScanner::Transformer* backup_transformer_;

  MessageHandler* handler_;

  FRIEND_TEST(AssociationTransformerTest, TransformsCorrectly);

  DISALLOW_COPY_AND_ASSIGN(AssociationTransformer);
};

// Extremely simple slot which just sets an association in a std::map when
// it is Render()ed. It associates the key (input URL) with this slot's
// resource URL (the output URL).
// Can be used to set AssociationTransformer::map() so that
// AssocitationTransformer::Transform() will rewrite the rendered URLs.
class AssociationSlot : public ResourceSlot {
 public:
  // Note: map must outlive AssociationSlot.
  AssociationSlot(ResourcePtr resource,
                  StringStringMap* map, const StringPiece& key)
      : ResourceSlot(resource), map_(map) {
    key.CopyToString(&key_);
  }
  virtual ~AssociationSlot();

  // All Render() calls are from the same thread, so this doesn't need to be
  // thread-safe.
  virtual void Render() {
    (*map_)[key_] = resource()->url();
  }

  virtual GoogleString LocationString() {
    // TODO(sligocki): Improve quality of this diagnostic.
    // Also improve CssResourceSlot::LocationString() which is identical.
    return "Inside CSS";
  }

 private:
  StringStringMap* map_;
  GoogleString key_;

  DISALLOW_COPY_AND_ASSIGN(AssociationSlot);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ASSOCIATION_TRANSFORMER_H_
