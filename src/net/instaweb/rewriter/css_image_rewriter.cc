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
#include "net/instaweb/rewriter/public/img_rewrite_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

// Statistics names.
const char CssImageRewriter::kImageRewrites[] = "css_image_rewrites";
const char CssImageRewriter::kCacheExtends[] = "css_image_cache_extends";
const char CssImageRewriter::kNoRewrite[] = "css_image_no_rewrite";

CssImageRewriter::CssImageRewriter(RewriteDriver* driver,
                                   CacheExtender* cache_extender,
                                   ImgRewriteFilter* image_rewriter)
    : driver_(driver),
      // For now we use the same options as for rewriting and cache-extending
      // images found in HTML.
      cache_extender_(cache_extender),
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
          options->Enabled(RewriteOptions::kExtendCache));
}

bool CssImageRewriter::RewriteImageUrl(const GoogleUrl& base_url,
                                       const StringPiece& old_rel_url,
                                       std::string* new_url,
                                       int64* expire_at_ms,
                                       MessageHandler* handler) {
  bool ret = false;
  *expire_at_ms = kint64max;
  std::string old_rel_url_str = old_rel_url.as_string();
  GoogleUrl resource_url(base_url, old_rel_url);
  scoped_ptr<Resource> input_resource(
      driver_->CreateInputResource(resource_url));
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
      *expire_at_ms = ExpirationTimeMs(rewrite_info.get());
      if (rewrite_info.get() != NULL && rewrite_info->optimizable()) {
        if (image_rewrites_ != NULL) {
          image_rewrites_->Add(1);
        }
        *new_url = rewrite_info->url();
        ret = true;
      }
    }
    // Try cache extending.
    if (!ret && options->Enabled(RewriteOptions::kExtendCache)) {
      handler->Message(kInfo, "Attempting to cache extend image %s",
                       old_rel_url_str.c_str());
      rewrite_info.reset(
          cache_extender_->RewriteExternalResource(input_resource.get(), NULL));
      *expire_at_ms = std::min(*expire_at_ms,
                               ExpirationTimeMs(rewrite_info.get()));

      if (rewrite_info.get() != NULL && rewrite_info->optimizable()) {
        if (cache_extends_ != NULL) {
          cache_extends_->Add(1);
        }
        *new_url = rewrite_info->url();
        ret = true;
      }
    }

    // Try trimming the URL.
    if (options->Enabled(RewriteOptions::kLeftTrimUrls)) {
      StringPiece url_to_trim;
      if (ret) {
        url_to_trim = *new_url;
      } else {
        url_to_trim = old_rel_url;
      }
      std::string trimmed_url;
      if (UrlLeftTrimFilter::Trim(base_url, url_to_trim,
                                  &trimmed_url, handler)) {
        *new_url = trimmed_url;
        ret = true;
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

bool CssImageRewriter::RewriteCssImages(const GoogleUrl& base_url,
                                        Css::Stylesheet* stylesheet,
                                        int64* expiration_time_ms,
                                        MessageHandler* handler) {
  bool edited = false;
  int64 expire_at_ms = kint64max;
  if (RewritesEnabled()) {
    handler->Message(kInfo, "Starting to rewrite images in CSS in %s",
                     base_url.spec_c_str());
    Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
    for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
         ruleset_iter != rulesets.end(); ++ruleset_iter) {
      Css::Ruleset* ruleset = *ruleset_iter;
      Css::Declarations& decls = ruleset->mutable_declarations();
      for (Css::Declarations::iterator decl_iter = decls.begin();
           decl_iter != decls.end(); ++decl_iter) {
        Css::Declaration* decl = *decl_iter;
        // Only edit image declarations.
        switch (decl->prop()) {
          case Css::Property::BACKGROUND:
          case Css::Property::BACKGROUND_IMAGE:
          case Css::Property::LIST_STYLE:
          case Css::Property::LIST_STYLE_IMAGE: {
            // Rewrite all URLs. Technically, background-image should only
            // have a single value which is a URL, but background could have
            // more values.
            Css::Values* values = decl->mutable_values();
            for (Css::Values::iterator value_iter = values->begin();
                 value_iter != values->end(); ++value_iter) {
              Css::Value* value = *value_iter;
              if (value->GetLexicalUnitType() == Css::Value::URI) {
                std::string rel_url =
                    UnicodeTextToUTF8(value->GetStringValue());
                handler->Message(kInfo, "Found image URL %s", rel_url.c_str());
                std::string new_url;
                int64 input_expire_at_ms;
                if (RewriteImageUrl(base_url, rel_url, &new_url,
                                    &input_expire_at_ms, handler)) {
                  // Replace the URL.
                  *value_iter = new Css::Value(Css::Value::URI,
                                               UTF8ToUnicodeText(new_url));
                  delete value;
                  edited = true;
                  handler->Message(kInfo, "Successfully rewrote %s to %s",
                                   rel_url.c_str(), new_url.c_str());
                } else {
                  if (no_rewrite_ != NULL) {
                    no_rewrite_->Add(1);
                  }
                  handler->Message(kInfo, "Could not rewrite %s "
                                   "(Perhaps it is being fetched).",
                                   rel_url.c_str());
                }
                expire_at_ms = std::min(expire_at_ms, input_expire_at_ms);
              }
            }
            break;
          }
          default:
            break;
        }
      }
    }
  } else {
    handler->Message(kInfo, "Image rewriting and cache extension not enabled, "
                     "so not rewriting images in CSS in %s",
                     base_url.spec_c_str());
  }
  *expiration_time_ms = expire_at_ms;
  return edited;
}

}  // namespace net_instaweb
