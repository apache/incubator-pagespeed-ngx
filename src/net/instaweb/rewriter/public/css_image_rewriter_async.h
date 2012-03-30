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

namespace Css {

class Values;

}  // namespace Css

namespace net_instaweb {

class CacheExtender;
class CssHierarchy;
class GoogleUrl;
class ImageCombineFilter;
class ImageRewriteFilter;
class MessageHandler;
class RewriteContext;
class RewriteDriver;
class Statistics;

class CssImageRewriterAsync {
 public:
  CssImageRewriterAsync(CssFilter::Context* context,
                        CssFilter* filter,
                        RewriteDriver* driver,
                        CacheExtender* cache_extender,
                        ImageRewriteFilter* image_rewriter,
                        ImageCombineFilter* image_combiner);
  ~CssImageRewriterAsync();

  static void Initialize(Statistics* statistics);

  // Attempts to rewrite the given CSS, starting nested rewrites for each
  // import and image to be rewritten. If successful, it mutates the CSS
  // to point to new images and flattens all @imports (if enabled).
  // Returns true if rewriting is enabled.
  bool RewriteCss(int64 image_inline_max_bytes,
                  RewriteContext* parent,
                  CssHierarchy* hierarchy,
                  MessageHandler* handler);

  // Is @import flattening enabled?
  bool FlatteningEnabled() const;

  // Are any rewrites enabled?
  bool RewritesEnabled(int64 image_inline_max_bytes) const;

 private:
  void RewriteImport(RewriteContext* parent,
                     CssHierarchy* hierarchy);

  void RewriteImage(int64 image_inline_max_bytes,
                    const GoogleUrl& trim_url,
                    const GoogleUrl& original_url,
                    RewriteContext* parent,
                    Css::Values* values, size_t value_index,
                    MessageHandler* handler);

  // Needed for import flattening.
  CssFilter* filter_;

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
