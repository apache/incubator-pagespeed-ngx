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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_

#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class CacheExtender;
class CachedResult;
class GoogleUrl;
class ImageRewriteFilter;
class MessageHandler;
class RewriteDriver;
class Statistics;
class Variable;

class CssImageRewriter {
 public:
  CssImageRewriter(RewriteDriver* driver,
                   CacheExtender* cache_extender,
                   ImageRewriteFilter* image_rewriter);
  ~CssImageRewriter();

  static void Initialize(Statistics* statistics);

  // Attempts to rewrite all images in stylesheet. If successful, it mutates
  // stylesheet to point to new images.
  //
  // Returns whether or not it made any changes.  The expiry of the answer is
  // the minimum of the expiries of all subresources in the stylesheet, or
  // kint64max if there are none)
  TimedBool RewriteCssImages(const GoogleUrl& base_url,
                             Css::Stylesheet* stylesheet,
                             MessageHandler* handler);

  // Are any rewrites enabled?
  bool RewritesEnabled() const;

  // Statistics names.
  static const char kImageRewrites[];
  static const char kCacheExtends[];
  static const char kNoRewrite[];

 private:
  TimedBool RewriteImageUrl(const GoogleUrl& base_url,
                            const StringPiece& old_rel_url,
                            GoogleString* new_url,
                            MessageHandler* handler);

  // Tells when we should expire our output based on a cached_result
  // produced from the rewriter. If NULL, it will produce a short delay
  // to permit the input to finish loading.
  int64 ExpirationTimeMs(CachedResult* cached_result);

  // Needed for resource_manager and options.
  RewriteDriver* driver_;

  // Pointers to other HTML filters used to rewrite images.
  // TODO(sligocki): morlovich suggests separating this out as some
  // centralized API call like rewrite_driver_->RewriteImage().
  CacheExtender* cache_extender_;
  ImageRewriteFilter* image_rewriter_;

  // Statistics
  Variable* image_rewrites_;
  Variable* cache_extends_;
  Variable* no_rewrite_;

  DISALLOW_COPY_AND_ASSIGN(CssImageRewriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_
