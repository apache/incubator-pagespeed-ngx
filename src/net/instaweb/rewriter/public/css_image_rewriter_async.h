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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_ASYNC_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_ASYNC_H_

#include <cstddef>

#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"        // for StringPiece
namespace Css {

class Values;
class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class CacheExtender;
class GoogleUrl;
class ImageCombineFilter;
class ImageRewriteFilter;
class MessageHandler;
class RewriteDriver;
class Statistics;

class CssImageRewriterAsync {
 public:
  CssImageRewriterAsync(CssFilter::Context* context,
                        RewriteDriver* driver,
                        CacheExtender* cache_extender,
                        ImageRewriteFilter* image_rewriter,
                        ImageCombineFilter* image_combiner);
  ~CssImageRewriterAsync();

  static void Initialize(Statistics* statistics);

  // Attempts to rewrite all images in stylesheet, starting nested rewrites.
  // If successful, it mutates stylesheet to point to new images. base_url
  // is the base of the original CSS and is used to convert relative URLs to
  // their absolute form for fetching; trim_url is the base of the rewritten
  // CSS and is used to convert absolute URLs in the resulting CSS to their
  // relative form.
  void RewriteCssImages(int64 image_inline_max_bytes,
                        const GoogleUrl& base_url,
                        const GoogleUrl& trim_url,
                        const StringPiece& contents,
                        Css::Stylesheet* stylesheet,
                        MessageHandler* handler);

  // Are any rewrites enabled?
  bool RewritesEnabled(int64 image_inline_max_bytes) const;

 private:
  void RewriteImage(int64 image_inline_max_bytes,
                    const GoogleUrl& trim_url,
                    const GoogleUrl& original_url,
                    Css::Values* values, size_t value_index,
                    MessageHandler* handler);

  // Needed for resource_manager and options.
  RewriteDriver* driver_;

  // For parenting our nested contexts.
  CssFilter::Context* context_;

  // Pointers to other HTML filters used to rewrite images.
  // TODO(sligocki): morlovich suggests separating this out as some
  // centralized API call like rewrite_driver_->RewriteImage().
  CacheExtender* cache_extender_;
  ImageCombineFilter* image_combiner_;
  ImageRewriteFilter* image_rewriter_;

  DISALLOW_COPY_AND_ASSIGN(CssImageRewriterAsync);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_ASYNC_H_
