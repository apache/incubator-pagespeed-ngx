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

#include "net/instaweb/rewriter/public/css_image_rewriter.h"

#include <algorithm>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

// Statistics names.
const char CssImageRewriter::kImageRewrites[] = "css_image_rewrites";
const char CssImageRewriter::kCacheExtends[] = "css_image_cache_extends";
const char CssImageRewriter::kNoRewrite[] = "css_image_no_rewrite";

CssImageRewriter::CssImageRewriter(RewriteDriver* driver,
                                   CacheExtender* cache_extender,
                                   ImageRewriteFilter* image_rewriter,
                                   ImageCombineFilter* image_combiner)
    : driver_(driver),
      // For now we use the same options as for rewriting and cache-extending
      // images found in HTML.
      cache_extender_(cache_extender),
      image_combiner_(image_combiner),
      image_rewriter_(image_rewriter),
      image_rewrites_(NULL),
      cache_extends_(NULL),
      no_rewrite_(NULL) {
  Statistics* stats = driver_->resource_manager()->statistics();
  if (stats != NULL) {
    image_rewrites_ = stats->GetVariable(kImageRewrites);
    // TODO(sligocki): Should this be shared with CacheExtender or kept
    // separately? I think it's useful to know how many images were optimized
    // from CSS files, but people probably also want to know how many total
    // images were cache-extended.
    cache_extends_ = stats->GetVariable(kCacheExtends);
    no_rewrite_ = stats->GetVariable(kNoRewrite);
  }
}

CssImageRewriter::~CssImageRewriter() {}

void CssImageRewriter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kCacheExtends);
  statistics->AddVariable(kNoRewrite);
}

bool CssImageRewriter::RewritesEnabled() const {
  const RewriteOptions* options = driver_->options();
  return (options->Enabled(RewriteOptions::kRewriteImages) ||
          options->Enabled(RewriteOptions::kLeftTrimUrls) ||
          options->Enabled(RewriteOptions::kExtendCache) ||
          options->Enabled(RewriteOptions::kSpriteImages));
}

TimedBool CssImageRewriter::RewriteImageUrl(const GoogleUrl& base_url,
                                            const StringPiece& old_rel_url,
                                            GoogleString* new_url,
                                            MessageHandler* handler) {
  TimedBool ret = {kint64max, false};
  GoogleString old_rel_url_str = old_rel_url.as_string();
  GoogleUrl resource_url(base_url, old_rel_url);
  ResourcePtr input_resource(driver_->CreateInputResource(resource_url));
  const RewriteOptions* options = driver_->options();
  if (input_resource.get() != NULL) {
    scoped_ptr<CachedResult> rewrite_info;
    // Try image rewriting.
    if (options->Enabled(RewriteOptions::kRewriteImages)) {
      handler->Message(kInfo, "Attempting to rewrite image %s",
                       old_rel_url_str.c_str());
      ResourceContext dim;
      rewrite_info.reset(image_rewriter_->RewriteExternalResource(
          input_resource.get(), &dim));
      ret.expiration_ms = ExpirationTimeMs(rewrite_info.get());
      if (rewrite_info.get() != NULL && rewrite_info->optimizable()) {
        if (image_rewrites_ != NULL) {
          image_rewrites_->Add(1);
        }
        *new_url = rewrite_info->url();
        ret.value = true;
      }
    }
    // Try cache extending.
    if (!ret.value && options->Enabled(RewriteOptions::kExtendCache)) {
      handler->Message(kInfo, "Attempting to cache extend image %s",
                       old_rel_url_str.c_str());
      rewrite_info.reset(
          cache_extender_->RewriteExternalResource(input_resource.get(), NULL));
      ret.expiration_ms = std::min(ret.expiration_ms,
                           ExpirationTimeMs(rewrite_info.get()));

      if (rewrite_info.get() != NULL && rewrite_info->optimizable()) {
        if (cache_extends_ != NULL) {
          cache_extends_->Add(1);
        }
        *new_url = rewrite_info->url();
        ret.value = true;
      }
    }

    // Try trimming the URL.
    if (options->Enabled(RewriteOptions::kLeftTrimUrls)) {
      StringPiece url_to_trim;
      if (ret.value) {
        url_to_trim = *new_url;
      } else {
        url_to_trim = old_rel_url;
      }
      GoogleString trimmed_url;
      if (UrlLeftTrimFilter::Trim(base_url, url_to_trim,
                                  &trimmed_url, handler)) {
        *new_url = trimmed_url;
        ret.value = true;
      }
    }
  }
  return ret;
}

int64 CssImageRewriter::ExpirationTimeMs(CachedResult* cached_result) {
  if (cached_result == NULL) {
    // A NULL cached_result means that the rewrite was unable to proceed,
    // but will likely be able to do so shortly, so we want to expire
    // "almost immediately". We use 1 second as it's the smallest TTL
    // we can reliably represent, anyway.
    int now_ms = driver_->resource_manager()->timer()->NowMs();
    return now_ms + Timer::kSecondMs;
  } else {
    return cached_result->origin_expiration_time_ms();
  }
}

TimedBool CssImageRewriter::RewriteCssImages(const GoogleUrl& base_url,
                                             Css::Stylesheet* stylesheet,
                                             MessageHandler* handler) {
  image_combiner_->Reset();
  bool edited = false;
  bool spriting_ok = driver_->options()->Enabled(RewriteOptions::kSpriteImages);
  int64 expire_at_ms = kint64max;
  if (RewritesEnabled()) {
    handler->Message(kInfo, "Starting to rewrite images in CSS in %s",
                     base_url.spec_c_str());
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
                handler->Message(kInfo, "Found image URL %s", rel_url.c_str());
                GoogleString new_url;
                // TODO(abliss): only do this resolution once.
                const GoogleUrl original_url(base_url, rel_url);
                TimedBool result = {kint64max, false};
                if (spriting_ok) {
                  result = image_combiner_->AddCssBackground(
                      original_url, &decls, value, handler);
                }
                expire_at_ms = std::min(expire_at_ms, result.expiration_ms);
                if (result.value) {
                  // TODO(abliss): sharing between spriting and other rewrites.
                  // For now we assume that spriting subsumes all other rewrites
                  // -- i.e. cache extending and recompressing.  This is
                  // particularly bad news if there's exactly one image in the
                  // CSS, since we'll assume it's going to be sprited, but it
                  // won't be.
                } else {
                  result = RewriteImageUrl(base_url, rel_url, &new_url,
                                           handler);
                  expire_at_ms = std::min(expire_at_ms, result.expiration_ms);
                  if (result.value) {
                    // Replace the URL.
                    (*values)[value_index] = new Css::Value(
                        Css::Value::URI, UTF8ToUnicodeText(new_url));
                    delete value;
                    edited = true;
                    handler->Message(kInfo, "Successfully rewrote %s to %s",
                                     rel_url.c_str(), new_url.c_str());
                  }
                }
                if (!result.value)  {
                  if (no_rewrite_ != NULL) {
                    no_rewrite_->Add(1);
                  }
                  GoogleString time_str;
                  if (!ConvertTimeToString(result.expiration_ms, &time_str)) {
                    time_str = "?";
                  }
                  handler->Message(
                      kInfo, "Can't rewrite %s until %s "
                      "(Perhaps it is being fetched).",
                      rel_url.c_str(), time_str.c_str());
                }
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
  if (spriting_ok) {
    edited |= image_combiner_->DoCombine(handler);
  }
  TimedBool ret = {expire_at_ms, edited};
  return ret;
}

}  // namespace net_instaweb
