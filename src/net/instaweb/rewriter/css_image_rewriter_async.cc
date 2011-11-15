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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_image_rewriter_async.h"

#include <cstddef>
#include <vector>

#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_resource_slot.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/image_combine_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/value.h"

namespace net_instaweb {

CssImageRewriterAsync::CssImageRewriterAsync(CssFilter::Context* context,
                                             RewriteDriver* driver,
                                             CacheExtender* cache_extender,
                                             ImageRewriteFilter* image_rewriter,
                                             ImageCombineFilter* image_combiner)
    : driver_(driver),
      context_(context),
      // For now we use the same options as for rewriting and cache-extending
      // images found in HTML.
      cache_extender_(cache_extender),
      image_combiner_(image_combiner),
      image_rewriter_(image_rewriter) {
  // TODO(morlovich): Unlike the original CssImageRewriter, this uses the same
  // statistics as underlying filters like CacheExtender. Should it get separate
  // stats instead? sligocki thinks it's useful to know how many images were
  // optimized from CSS files, but people probably also want to know how many
  // total images were cache-extended.
}

CssImageRewriterAsync::~CssImageRewriterAsync() {}

bool CssImageRewriterAsync::RewritesEnabled() const {
  const RewriteOptions* options = driver_->options();
  return (options->Enabled(RewriteOptions::kRecompressImages) ||
          options->Enabled(RewriteOptions::kLeftTrimUrls) ||
          options->Enabled(RewriteOptions::kExtendCache) ||
          options->Enabled(RewriteOptions::kSpriteImages));
}

void CssImageRewriterAsync::RewriteImage(
    int64 image_inline_max_bytes,
    const GoogleUrl& trim_url,
    const GoogleUrl& original_url,
    Css::Values* values, size_t value_index,
    MessageHandler* handler) {
  const RewriteOptions* options = driver_->options();
  ResourcePtr resource = driver_->CreateInputResource(original_url);
  if (resource.get() == NULL) {
    return;
  }

  CssResourceSlotPtr slot(
      context_->slot_factory()->GetSlot(resource, values, value_index));

  if (options->Enabled(RewriteOptions::kRecompressImages) ||
      image_inline_max_bytes > 0) {
    context_->RegisterNested(
        image_rewriter_->MakeNestedRewriteContextForCss(image_inline_max_bytes,
            context_, ResourceSlotPtr(slot)));
  }

  if (options->Enabled(RewriteOptions::kExtendCache)) {
    context_->RegisterNested(
        cache_extender_->MakeNestedContext(context_, ResourceSlotPtr(slot)));
  }

  // TODO(sligocki): DomainRewriter or is this done automatically?

  if (options->trim_urls_in_css() &&
      options->Enabled(RewriteOptions::kLeftTrimUrls)) {
    // TODO(sligocki): Make sure this is the correct (final) URL of the CSS.
    slot->EnableTrim(trim_url);
  }
}

void CssImageRewriterAsync::RewriteCssImages(int64 image_inline_max_bytes,
                                             const GoogleUrl& base_url,
                                             const GoogleUrl& trim_url,
                                             const StringPiece& contents,
                                             Css::Stylesheet* stylesheet,
                                             MessageHandler* handler) {
  const RewriteOptions* options = driver_->options();
  bool spriting_ok = options->Enabled(RewriteOptions::kSpriteImages);

  if (RewritesEnabled() || image_inline_max_bytes > 0) {
    handler->Message(kInfo, "Starting to rewrite images in CSS in %s",
                     base_url.spec_c_str());
    if (spriting_ok) {
      image_combiner_->Reset(context_, base_url, contents);
    }
    Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
    for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
         ruleset_iter != rulesets.end(); ++ruleset_iter) {
      Css::Ruleset* ruleset = *ruleset_iter;
      Css::Declarations& decls = ruleset->mutable_declarations();
      bool background_position_found = false;
      bool background_image_found = false;
      for (Css::Declarations::iterator decl_iter = decls.begin();
           decl_iter != decls.end(); ++decl_iter) {
        Css::Declaration* decl = *decl_iter;
        // Only edit image declarations.
        switch (decl->prop()) {
          case Css::Property::BACKGROUND_POSITION:
          case Css::Property::BACKGROUND_POSITION_X:
          case Css::Property::BACKGROUND_POSITION_Y:
            background_position_found = true;
            break;
          case Css::Property::BACKGROUND:
          case Css::Property::BACKGROUND_IMAGE:
          case Css::Property::LIST_STYLE:
          case Css::Property::LIST_STYLE_IMAGE: {
            // Rewrite all URLs. Technically, background-image should only
            // have a single value which is a URL, but background could have
            // more values.
            Css::Values* values = decl->mutable_values();
            for (size_t value_index = 0; value_index < values->size();
                 value_index++) {
              Css::Value* value = values->at(value_index);
              if (value->GetLexicalUnitType() == Css::Value::URI) {
                background_image_found = true;
                GoogleString rel_url =
                    UnicodeTextToUTF8(value->GetStringValue());
                // TODO(abliss): only do this resolution once.
                const GoogleUrl original_url(base_url, rel_url);
                if (!original_url.is_valid()) {
                  handler->Message(kInfo, "Invalid URL %s", rel_url.c_str());
                  continue;
                }
                if (!options->IsAllowed(original_url.Spec())) {
                  handler->Message(kInfo, "Disallowed URL %s", rel_url.c_str());
                  continue;
                }
                handler->Message(kInfo, "Found image URL %s", rel_url.c_str());
                if (spriting_ok) {
                  image_combiner_->AddCssBackgroundContext(
                      original_url, values, value_index, context_,
                      &decls, handler);
                }
                RewriteImage(image_inline_max_bytes, trim_url, original_url,
                             values, value_index, handler);
              }
            }
            break;
          }
          default:
            break;
        }
      }
      // All the declarations in this ruleset have been parsed.
      if (spriting_ok && background_position_found && !background_image_found) {
        // A ruleset that contains a background-position but no background image
        // is a signal that we should not be spriting.
        handler->Message(kInfo,
                         "Lone background-position found: Cannot sprite.");
        spriting_ok = false;
      }
    }
  } else {
    handler->Message(kInfo, "Image rewriting and cache extension not enabled, "
                     "so not rewriting images in CSS in %s",
                     base_url.spec_c_str());
  }
}

}  // namespace net_instaweb
