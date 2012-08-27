/*
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/css_hierarchy.h"
#include "net/instaweb/rewriter/public/css_resource_slot.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class AssociationTransformer;
class CssImageRewriter;
class CacheExtender;
class HtmlCharactersNode;
class ImageCombineFilter;
class ImageRewriteFilter;
class MessageHandler;
class OutputPartitions;
class ResourceContext;
class RewriteContext;
class RewriteDomainTransformer;
class Statistics;
class UrlSegmentEncoder;
class Variable;
class Writer;

// Find and parse all CSS in the page and apply transformations including:
// minification, combining, refactoring, and optimizing sub-resources.
//
// Currently only does basic minification.
//
// Note that CssCombineFilter currently does combining (although there is a bug)
// but CssFilter will eventually replace this.
//
// Currently only deals with inline <style> tags and external <link> resources.
// It does not consider style= attributes on arbitrary elements.
class CssFilter : public RewriteFilter {
 public:
  class Context;

  CssFilter(RewriteDriver* driver,
            // TODO(sligocki): Temporary pattern until we figure out a better
            // way to do this without passing all filters around everywhere.
            CacheExtender* cache_extender,
            ImageRewriteFilter* image_rewriter,
            ImageCombineFilter* image_combiner);
  virtual ~CssFilter();

  static void Initialize(Statistics* statistics);
  static void Terminate();

  // Note: AtExitManager needs to be initialized or you get a nasty error:
  // Check failed: false. Tried to RegisterCallback without an AtExitManager.
  // This is called by Initialize.
  static void InitializeAtExitManager();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "CssFilter"; }
  virtual const char* id() const { return RewriteOptions::kCssFilterId; }
  virtual int FilterCacheFormatVersion() const;

  static const char kBlocksRewritten[];
  static const char kParseFailures[];
  static const char kFallbackRewrites[];
  static const char kFallbackFailures[];
  static const char kRewritesDropped[];
  static const char kTotalBytesSaved[];
  static const char kTotalOriginalBytes[];
  static const char kUses[];
  static const char kCharsetMismatch[];
  static const char kInvalidUrl[];
  static const char kLimitExceeded[];
  static const char kMinifyFailed[];
  static const char kRecursion[];

  RewriteContext* MakeNestedFlatteningContextInNewSlot(
      const ResourcePtr& resource, const GoogleString& location,
      CssFilter::Context* rewriter, RewriteContext* parent,
      CssHierarchy* hierarchy);

 protected:
  virtual RewriteContext* MakeRewriteContext();
  virtual const UrlSegmentEncoder* encoder() const;
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

 private:
  friend class Context;
  friend class CssFlattenImportsContext;  // for statistics
  friend class CssHierarchy;              // for statistics

  Context* MakeContext(RewriteDriver* driver,
                       RewriteContext* parent);

  // Starts the asynchronous rewrite process for inline CSS 'text'.
  void StartInlineRewrite(HtmlCharactersNode* text);

  // Starts the asynchronous rewrite process for inline CSS inside the given
  // element's given style attribute.
  void StartAttributeRewrite(HtmlElement* element,
                             HtmlElement::Attribute* style);

  // Starts the asynchronous rewrite process for external CSS referenced by
  // attribute 'src' of 'link'.
  void StartExternalRewrite(HtmlElement* link, HtmlElement::Attribute* src);

  ResourceSlot* MakeSlotForInlineCss(const StringPiece& content);
  CssFilter::Context* StartRewriting(const ResourceSlotPtr& slot);

  // Get the charset of the HTML being parsed which can be specified in the
  // driver's headers, defaulting to ISO-8859-1 if isn't. Then, if a charset
  // is specified in the given element, check that they agree, and if not
  // return false, otherwise return true and assign the first charset to the
  // given string.
  bool GetApplicableCharset(const HtmlElement* element,
                            GoogleString* charset) const;

  // Get the media specified in the given element, if any. Returns true if
  // media were found false if not.
  bool GetApplicableMedia(const HtmlElement* element,
                          StringVector* media) const;

  bool in_style_element_;  // Are we in a style element?
  // This is meaningless if in_style_element_ is false:
  HtmlElement* style_element_;  // The element we are in.

  // The charset extracted from a meta tag, if any.
  GoogleString meta_tag_charset_;

  // Filters we delegate to.
  CacheExtender* cache_extender_;
  ImageRewriteFilter* image_rewrite_filter_;
  ImageCombineFilter* image_combiner_;

  // Statistics
  // # of CSS blocks (CSS files, <style> blocks or style= attributes)
  // successfully rewritten.
  Variable* num_blocks_rewritten_;
  // # of CSS blocks that rewriter failed to parse.
  Variable* num_parse_failures_;
  // # of CSS blocks that failed to be parsed, but were rewritten in the
  // fallback path.
  Variable* num_fallback_rewrites_;
  // # of CSS blocks that failed to be rewritten in the fallback path.
  Variable* num_fallback_failures_;
  // # of CSS rewrites which were not applied because they made the CSS larger
  // and did not rewrite any images in it/flatten any other CSS files into it.
  Variable* num_rewrites_dropped_;
  // # of bytes saved from rewriting CSS (including minification and the
  // increase of bytes from longer image URLs and the increase of bytes
  // from @import flattening).
  // TODO(sligocki): This should consider the input size to be the input sizes
  // of all CSS files flattened into this one. Currently it does not.
  Variable* total_bytes_saved_;
  // Sum of original bytes of all successfully rewritten CSS blocks.
  // total_bytes_saved_ / total_original_bytes_ should be the
  // average percentage reduction of CSS block size.
  Variable* total_original_bytes_;
  // # of uses of rewritten CSS (updating <link> href= attributes,
  // <style> contents or style= attributes).
  Variable* num_uses_;
  // # of times CSS was not flattened because of a charset mismatch.
  Variable* num_flatten_imports_charset_mismatch_;
  // # of times CSS was not flattened because of an invalid @import URL.
  Variable* num_flatten_imports_invalid_url_;
  // # of times CSS was not flattened because the resulting CSS too big.
  Variable* num_flatten_imports_limit_exceeded_;
  // # of times CSS was not flattened because minification failed.
  Variable* num_flatten_imports_minify_failed_;
  // # of times CSS was not flattened because of recursive imports.
  Variable* num_flatten_imports_recursion_;

  CssUrlEncoder encoder_;

  DISALLOW_COPY_AND_ASSIGN(CssFilter);
};

// Context used by CssFilter under async flow.
class CssFilter::Context : public SingleRewriteContext {
 public:
  Context(CssFilter* filter, RewriteDriver* driver,
          RewriteContext* parent,
          CacheExtender* cache_extender,
          ImageRewriteFilter* image_rewriter,
          ImageCombineFilter* image_combiner,
          ResourceContext* context);
  virtual ~Context();

  // Setup rewriting for inline, attribute, or external CSS.
  void SetupInlineRewrite(HtmlElement* style_element, HtmlCharactersNode* text);
  void SetupAttributeRewrite(HtmlElement* element, HtmlElement::Attribute* src);
  void SetupExternalRewrite(const GoogleUrl& base_gurl,
                            const GoogleUrl& trim_gurl);

  // Starts nested rewrite jobs for any imports or images contained in the CSS.
  // Marked public, so that it's accessible from CssHierarchy.
  void RewriteCssFromNested(RewriteContext* parent, CssHierarchy* hierarchy);

  // Specialization to absolutify URLs in input resource in case of rewrite
  // fail or deadline exceeded.
  virtual bool AbsolutifyIfNeeded(const StringPiece& input_contents,
                                  Writer* writer, MessageHandler* handler);

  CssResourceSlotFactory* slot_factory() { return &slot_factory_; }

  CssHierarchy* mutable_hierarchy() { return &hierarchy_; }

 protected:
  virtual void Render();
  virtual void Harvest();
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs);
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual GoogleString CacheKeySuffix() const;
  virtual const UrlSegmentEncoder* encoder() const;

 private:
  bool RewriteCssText(const GoogleUrl& css_base_gurl,
                      const GoogleUrl& css_trim_gurl,
                      const StringPiece& in_text,
                      int64 in_text_size,
                      bool text_is_declarations,
                      MessageHandler* handler);

  // Starts nested rewrite jobs for any imports or images contained in the CSS.
  void RewriteCssFromRoot(const StringPiece& in_text, int64 in_text_size,
                          bool has_unparseables, Css::Stylesheet* stylesheet);

  // Fall back to using CssTagScanner to find the URLs and rewrite them
  // that way. Like RewriteCssFromRoot, output is written into output
  // resource in Harvest(). Called if CSS Parser fails to parse doc.
  // Returns whether or not fallback rewriting succeeds. Fallback can fail
  // if URLs in CSS are not parseable.
  bool FallbackRewriteUrls(const StringPiece& in_text);

  // Tries to write out a (potentially edited) stylesheet out to out_text,
  // and returns whether we should consider the result as an improvement.
  bool SerializeCss(int64 in_text_size,
                    const Css::Stylesheet* stylesheet,
                    const GoogleUrl& css_base_gurl,
                    const GoogleUrl& css_trim_gurl,
                    bool previously_optimized,
                    bool stylesheet_is_declarations,
                    bool add_utf8_bom,
                    GoogleString* out_text,
                    MessageHandler* handler);

  // Used by the asynchronous rewrite callbacks (RewriteSingle + Harvest) to
  // determine if what is being rewritten is a style attribute or a stylesheet,
  // since an attribute comprises only declarations, unlike a stlyesheet.
  bool IsInlineAttribute() const {
    return (rewrite_inline_attribute_ != NULL);
  }

  // Determine the appropriate image inlining threshold based upon whether we're
  // in an html file (<style> tag or style= attribute) or in an external css
  // file.
  int64 ImageInlineMaxBytes() const {
    if (rewrite_inline_element_ != NULL) {
      // We're in an html context.
      return driver_->options()->ImageInlineMaxBytes();
    } else {
      // We're in a standalone CSS file.
      return driver_->options()->CssImageInlineMaxBytes();
    }
  }

  CssFilter* filter_;
  RewriteDriver* driver_;
  scoped_ptr<CssImageRewriter> css_image_rewriter_;
  CssResourceSlotFactory slot_factory_;
  CssHierarchy hierarchy_;
  bool css_rewritten_;
  bool has_utf8_bom_;

  // Are we performing a fallback rewrite?
  bool fallback_mode_;
  // Transformer used by CssTagScanner to rewrite URLs if we failed to
  // parse CSS. This will only be defined if CSS parsing failed.
  scoped_ptr<AssociationTransformer> fallback_transformer_;
  // Backup transformer for AssociationTransformer. Absolutifies URLs and
  // rewrites their domains as necessary if they can't be cache extended.
  scoped_ptr<RewriteDomainTransformer> absolutifier_;

  // Style element containing inline CSS (see StartInlineRewrite) -or-
  // any element with a style attribute (see StartAttributeRewrite), or
  // NULL if we're rewriting external stuff.
  HtmlElement* rewrite_inline_element_;

  // Node with inline CSS to rewrite, or NULL if we're rewriting external stuff.
  HtmlCharactersNode* rewrite_inline_char_node_;

  // The style attribute associated with rewrite_inline_element_. Mutually
  // exclusive with rewrite_inline_char_node_ since style elements cannot
  // have style attributes.
  HtmlElement::Attribute* rewrite_inline_attribute_;

  // Information needed for nested rewrites or finishing up serialization.
  int64 in_text_size_;
  GoogleUrl css_base_gurl_;
  GoogleUrl css_trim_gurl_;
  ResourcePtr input_resource_;
  OutputResourcePtr output_resource_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_
