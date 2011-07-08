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

#include "net/instaweb/rewriter/public/css_filter.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_image_rewriter.h"
#include "net/instaweb/rewriter/public/css_image_rewriter_async.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

#include "base/at_exit.h"

namespace {

base::AtExitManager* at_exit_manager = NULL;

}  // namespace

namespace net_instaweb {
class CacheExtender;
class ImageCombineFilter;
class ImageRewriteFilter;
class MessageHandler;
class RewriteContext;

namespace {

const char kStylesheet[] = "stylesheet";

// A slot we use when rewriting inline CSS --- there is no place or need
// to write out an output URL, so it has a no-op Render().
class InlineCssSlot : public ResourceSlot {
 public:
  explicit InlineCssSlot(const ResourcePtr& resource)
      : ResourceSlot(resource) {}
  virtual ~InlineCssSlot() {}
  virtual void Render() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(InlineCssSlot);
};

}  // namespace

// Statistics variable names.
const char CssFilter::kFilesMinified[] = "css_filter_files_minified";
const char CssFilter::kMinifiedBytesSaved[] = "css_filter_minified_bytes_saved";
const char CssFilter::kParseFailures[] = "css_filter_parse_failures";

CssFilter::Context::Context(CssFilter* filter, RewriteDriver* driver,
                            CacheExtender* cache_extender,
                            ImageRewriteFilter* image_rewriter,
                            ImageCombineFilter* image_combiner)
    : SingleRewriteContext(driver, NULL /* no parent */,
                           NULL /* no resource context */),
      filter_(filter),
      driver_(driver),
      image_rewriter_(
          new CssImageRewriterAsync(
              this, driver, cache_extender, image_rewriter, image_combiner)),
      have_nested_rewrites_(false),
      rewrite_inline_element_(NULL),
      rewrite_inline_char_node_(NULL),
      in_text_size_(-1) {
}

CssFilter::Context::~Context() {
}

void CssFilter::Context::Render() {
  if (num_output_partitions() == 0) {
    return;
  }

  const CachedResult& result = output_partition(0)->result();
  if (rewrite_inline_char_node_ != NULL && result.optimizable()) {
    HtmlCharactersNode* new_style_char_node =
        driver_->NewCharactersNode(rewrite_inline_element_,
                                   result.inlined_data());
    driver_->ReplaceNode(rewrite_inline_char_node_, new_style_char_node);
  }
}

void CssFilter::Context::StartInlineRewrite(HtmlElement* style_element,
                                            HtmlCharactersNode* text) {
  // To handle nested rewrites of inline CSS, we internally handle it
  // as a rewrite of a data: URL.
  css_base_gurl_.Reset(driver_->base_url());
  rewrite_inline_element_ = style_element;
  rewrite_inline_char_node_ = text;

  GoogleString data_url;
  // TODO(morlovich): This does a lot of useless conversions and
  // copying. Get rid of them.
  DataUrl(kContentTypeCss, PLAIN, text->contents(), &data_url);
  ResourcePtr input_resource(DataUrlInputResource::Make(data_url, Manager()));
  ResourceSlotPtr slot(new InlineCssSlot(input_resource));
  AddSlot(slot);
  driver_->InitiateRewrite(this);
}

void CssFilter::Context::StartExternalRewrite(HtmlElement* link,
                                              HtmlElement::Attribute* src) {
  ResourcePtr input_resource(filter_->CreateInputResource(src->value()));
  if (input_resource.get() != NULL) {
    css_base_gurl_.Reset(input_resource->url());
    ResourceSlotPtr slot(driver_->GetSlot(input_resource, link, src));
    AddSlot(slot);
    driver_->InitiateRewrite(this);
  } else {
    delete this;
  }
}

void CssFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  input_resource_ = input_resource;
  output_resource_ = output_resource;

  TimedBool result = filter_->RewriteCssText(
      this, css_base_gurl_, input_resource->contents(),
      NULL /* out_text --- not written in RewriteCssText in async case */,
      driver_->message_handler());

  if (result.value) {
    if (have_nested_rewrites_) {
      StartNestedTasks();
    } else {
      // We call Harvest() ourselves so we can centralize all the output there.
      Harvest();
    }
  } else {
    RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
  }
}

void CssFilter::Context::RewriteImages(int64 in_text_size,
                                       Css::Stylesheet* stylesheet) {
  in_text_size_ = in_text_size;
  stylesheet_.reset(stylesheet);

  image_rewriter_->RewriteCssImages(css_base_gurl_, stylesheet,
                                    driver_->message_handler());
}

void CssFilter::Context::RegisterNested(RewriteContext* nested) {
  have_nested_rewrites_ = true;
  AddNestedContext(nested);
}

void CssFilter::Context::Harvest() {
  GoogleString out_text;
  // TODO(morlovich): Propagate whether we previously optimized properly
  // from the nested rewrites.
  bool ok = filter_->SerializeCss(
      in_text_size_, stylesheet_.get(), css_base_gurl_,
      false /* previously_optimized */, &out_text,
      driver_->message_handler());
  if (ok) {
    if (rewrite_inline_char_node_ == NULL) {
      // TODO(morlovich): Incorporate time from nested rewrites.
      int64 expire_ms = input_resource_->CacheExpirationTimeMs();
      output_resource_->SetType(&kContentTypeCss);
      ok = Manager()->Write(HttpStatus::kOK, out_text,
                            output_resource_.get(),
                            expire_ms, Driver()->message_handler());
    } else {
      output_partition(0)->mutable_result()->set_inlined_data(out_text);
    }
  }

  if (ok) {
    RewriteDone(RewriteSingleResourceFilter::kRewriteOk, 0);
  } else {
    RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
  }
}

bool CssFilter::Context::Partition(OutputPartitions* partitions,
                                   OutputResourceVector* outputs) {
  if (rewrite_inline_char_node_ == NULL) {
    return SingleRewriteContext::Partition(partitions, outputs);
  } else {
    // In case where we're rewriting inline CSS, we don't want an output
    // resource but still want a non-trivial partition.
    OutputPartition* partition = partitions->add_partition();
    slot(0)->resource()->AddInputInfoToPartition(0, partition);
    outputs->push_back(OutputResourcePtr(NULL));
    return true;
  }
}

GoogleString CssFilter::Context::CacheKey() const {
  GoogleString key;
  if (rewrite_inline_char_node_ != NULL) {
    // When rewriting inline CSS we pack all the data inside the URL, which
    // is too long to sensibly use as a cache key; so we shorten it via a hash.
    //
    // We also incorporate the base path of the HTML as part of the key --- it
    // matters  for inline CSS since resources are resolved against that (while
    // it doesn't for external CSS, since that uses the stylesheet as the base).
    MD5Hasher hasher;
    GoogleString raw_key =
      StrCat("data-key:", hasher.Hash(slot(0)->resource()->url()),
             "@", css_base_gurl_.AllExceptLeaf());
    UrlEscaper::EncodeToUrlSegment(raw_key, &key);
  } else {
    key = SingleRewriteContext::CacheKey();
  }

  // We want to incorporate various of our settings inside our cache key,
  // so if our configuration changes (like due to a different .htaccess)
  // we do not end up serving the wrong thing. We don't want it inside
  // the URL, however, since it's not critical for reconstructing the resource.
  //
  // TODO(morlovich): Make the quirks bit part of the actual output resource
  // name; as ignoring it on the fetch path is unsafe.
  const RewriteOptions* options = driver_->options();
  StrAppend(&key,
            options->always_rewrite_css() ? "A" : "m",
            driver_->doctype().IsXhtml() ? "X" : "h",
            options->Enabled(RewriteOptions::kRecompressImages) ? "R" : "_",
            options->Enabled(RewriteOptions::kLeftTrimUrls) ? "T" : "_",
            options->Enabled(RewriteOptions::kExtendCache) ? "E" : "_",
            options->Enabled(RewriteOptions::kSpriteImages) ? "S" : "_");
  return key;
}

CssFilter::CssFilter(RewriteDriver* driver, const StringPiece& path_prefix,
                     CacheExtender* cache_extender,
                     ImageRewriteFilter* image_rewriter,
                     ImageCombineFilter* image_combiner)
    : RewriteSingleResourceFilter(driver, path_prefix),
      in_style_element_(false),
      image_rewriter_(new CssImageRewriter(driver, cache_extender,
                                           image_rewriter, image_combiner)),
      cache_extender_(cache_extender),
      image_rewrite_filter_(image_rewriter),
      image_combiner_(image_combiner),
      num_files_minified_(NULL),
      minified_bytes_saved_(NULL),
      num_parse_failures_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    num_files_minified_ = stats->GetVariable(CssFilter::kFilesMinified);
    minified_bytes_saved_ = stats->GetVariable(CssFilter::kMinifiedBytesSaved);
    num_parse_failures_ = stats->GetVariable(CssFilter::kParseFailures);
  }
}

CssFilter::~CssFilter() {}

int CssFilter::FilterCacheFormatVersion() const {
  return 1;
}

void CssFilter::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(CssFilter::kFilesMinified);
    statistics->AddVariable(CssFilter::kMinifiedBytesSaved);
    statistics->AddVariable(CssFilter::kParseFailures);
    CssImageRewriter::Initialize(statistics);
  }

  InitializeAtExitManager();
}

void CssFilter::Terminate() {
  // Note: This is not thread-safe, but I don't believe we need it to be.
  if (at_exit_manager != NULL) {
    delete at_exit_manager;
    at_exit_manager = NULL;
  }
}

void CssFilter::InitializeAtExitManager() {
  // Note: This is not thread-safe, but I don't believe we need it to be.
  if (at_exit_manager == NULL) {
    at_exit_manager = new base::AtExitManager;
  }
}

void CssFilter::StartDocumentImpl() {
  in_style_element_ = false;
}

void CssFilter::StartElementImpl(HtmlElement* element) {
  // HtmlParse should not pass us elements inside a style element.
  CHECK(!in_style_element_);
  if (element->keyword() == HtmlName::kStyle) {
    in_style_element_ = true;
    style_element_ = element;
    style_char_node_ = NULL;
  }
  // We deal with <link> elements in EndElement.
}

void CssFilter::Characters(HtmlCharactersNode* characters_node) {
  if (in_style_element_) {
    if (style_char_node_ == NULL) {
      style_char_node_ = characters_node;
    } else {
      driver_->ErrorHere("Multiple character nodes in style.");
      in_style_element_ = false;
    }
  }
}

void CssFilter::EndElementImpl(HtmlElement* element) {
  // Rewrite an inline style.
  if (in_style_element_) {
    CHECK(style_element_ == element);  // HtmlParse should not pass unmatching.

    if (driver_->IsRewritable(element) && style_char_node_ != NULL) {
      CHECK(element == style_char_node_->parent());  // Sanity check.
      GoogleString new_content;

      if (HasAsyncFlow()) {
        Context* context = MakeContext();
        context->StartInlineRewrite(element, style_char_node_);
      } else if (RewriteCssText(NULL /* no async context*/, driver_->base_url(),
                                style_char_node_->contents(), &new_content,
                                driver_->message_handler()).value) {
        // Note: Copy of new_content here.
        HtmlCharactersNode* new_style_char_node =
            driver_->NewCharactersNode(element, new_content);
        driver_->ReplaceNode(style_char_node_, new_style_char_node);
      }
    }
    in_style_element_ = false;

  // Rewrite an external style.
  } else if (element->keyword() == HtmlName::kLink &&
             driver_->IsRewritable(element)) {
    StringPiece relation(element->AttributeValue(HtmlName::kRel));
    if (relation == kStylesheet) {
      HtmlElement::Attribute* element_href = element->FindAttribute(
          HtmlName::kHref);
      if (element_href != NULL) {
        // If it has a href= attribute
        if (HasAsyncFlow()) {
          Context* context = MakeContext();
          context->StartExternalRewrite(element, element_href);
        } else {
          GoogleString new_url;
          if (RewriteExternalCss(element_href->value(), &new_url)) {
            element_href->SetValue(new_url);  // Update the href= attribute.
          }
        }
      } else {
        driver_->ErrorHere("Link element with no href.");
      }
    }
  }
}

// Return value answers the question: May we rewrite?
// If return false, out_text is undefined.
// css_gurl is the URL used to resolve relative URLs in the CSS.
// Specifically, it should be the address of the CSS document itself for
// external CSS or the HTML document that the CSS is in for inline CSS.
// The expiry of the answer is the minimum of the expiries of all subresources
// in the stylesheet, or kint64max if there are none or the sheet is invalid.
TimedBool CssFilter::RewriteCssText(Context* context,
                                    const GoogleUrl& css_gurl,
                                    const StringPiece& in_text,
                                    GoogleString* out_text,
                                    MessageHandler* handler) {
  int64 in_text_size = static_cast<int64>(in_text.size());
  // Load stylesheet w/o expanding background attributes and preserving all
  // values from original document.
  Css::Parser parser(in_text);
  parser.set_allow_all_values(true);
  // If we think this is XHTML, turn off quirks-mode so that we don't "fix"
  // things we shouldn't.
  // TODO(sligocki): We might need to do this in other cases too.
  if (driver_->doctype().IsXhtml()) {
    parser.set_quirks_mode(false);
  }
  scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

  TimedBool ret = {kint64max, true};
  if (stylesheet.get() == NULL ||
      parser.errors_seen_mask() != Css::Parser::kNoError) {
    ret.value = false;
    driver_->InfoHere("CSS parsing error in %s", css_gurl.spec_c_str());
    num_parse_failures_->Add(1);
  } else {
    // Edit stylesheet.
    if (HasAsyncFlow()) {
      // Start any nested rewrite tasks
      context->RewriteImages(in_text_size, stylesheet.release());

      // Rewrite OK thus far.
      ret.value = true;
      return ret;
    } else {
      TimedBool result = image_rewriter_->RewriteCssImages(
                             css_gurl, stylesheet.get(), handler);
      ret.expiration_ms = result.expiration_ms;
      ret.value = SerializeCss(in_text_size, stylesheet.get(), css_gurl,
                               result.value, out_text, handler);
    }
  }
  return ret;
}

bool CssFilter::SerializeCss(int64 in_text_size,
                             const Css::Stylesheet* stylesheet,
                             const GoogleUrl& css_gurl,
                             bool previously_optimized,
                             GoogleString* out_text,
                             MessageHandler* handler) {
  bool ret = true;

  // Re-serialize stylesheet.
  StringWriter writer(out_text);
  CssMinify::Stylesheet(*stylesheet, &writer, handler);

  // Get signed versions so that we can subtract them.
  int64 out_text_size = static_cast<int64>(out_text->size());
  int64 bytes_saved = in_text_size - out_text_size;

  if (!driver_->options()->always_rewrite_css()) {
    // Don't rewrite if we didn't edit it or make it any smaller.
    if (!previously_optimized && bytes_saved <= 0) {
      ret = false;
      driver_->InfoHere("CSS parser increased size of CSS file %s by %s "
                        "bytes.", css_gurl.spec_c_str(),
                        Integer64ToString(-bytes_saved).c_str());
    }
    // Don't rewrite if we blanked the CSS file! (This is a parse error)
    // TODO(sligocki): Don't error if in_text is all whitespace.
    if (out_text_size == 0 && in_text_size != 0) {
      ret = false;
      driver_->InfoHere("CSS parsing error in %s", css_gurl.spec_c_str());
      num_parse_failures_->Add(1);
    }
  }

  // Statistics
  if (ret) {
    driver_->InfoHere("Successfully rewrote CSS file %s saving %s "
                      "bytes.", css_gurl.spec_c_str(),
                      Integer64ToString(bytes_saved).c_str());
    num_files_minified_->Add(1);
    minified_bytes_saved_->Add(bytes_saved);
  }
  return ret;
}

// Combine all 'original_stylesheets' (and all their sub stylescripts) into a
// single returned stylesheet which has no @imports or returns NULL if we fail
// to load some sub-resources.
//
// Note: we must cannibalize input stylesheets or we will have ownership
// problems or a lot of deep-copying.
Css::Stylesheet* CssFilter::CombineStylesheets(
    std::vector<Css::Stylesheet*>* original_stylesheets) {
  // Load all sub-stylesheets to assure that we can do the combination.
  std::vector<Css::Stylesheet*> stylesheets;
  std::vector<Css::Stylesheet*>::const_iterator iter;
  for (iter = original_stylesheets->begin();
       iter < original_stylesheets->end(); ++iter) {
    Css::Stylesheet* stylesheet = *iter;
    if (!LoadAllSubStylesheets(stylesheet, &stylesheets)) {
      return NULL;
    }
  }

  // Once all sub-stylesheets are loaded in memory, combine them.
  Css::Stylesheet* combination = new Css::Stylesheet;
  // TODO(sligocki): combination->rulesets().reserve(...);
  for (std::vector<Css::Stylesheet*>::const_iterator iter = stylesheets.begin();
       iter < stylesheets.end(); ++iter) {
    Css::Stylesheet* stylesheet = *iter;
    // Append all rulesets from 'stylesheet' to 'combination' ...
    combination->mutable_rulesets().insert(
        combination->mutable_rulesets().end(),
        stylesheet->rulesets().begin(),
        stylesheet->rulesets().end());
    // ... and then clear rules from 'stylesheet' to avoid double ownership.
    stylesheet->mutable_rulesets().clear();
  }
  return combination;
}

// Collect a list of all stylesheets @imported by base_stylesheet directly or
// indirectly in the order that they will be dealt with by a CSS parser and
// append them to vector 'all_stylesheets'.
bool CssFilter::LoadAllSubStylesheets(
    Css::Stylesheet* base_stylesheet,
    std::vector<Css::Stylesheet*>* all_stylesheets) {
  const Css::Imports& imports = base_stylesheet->imports();
  for (Css::Imports::const_iterator iter = imports.begin();
       iter < imports.end(); ++iter) {
    Css::Import* import = *iter;
    StringPiece url(import->link.utf8_data(), import->link.utf8_length());

    // Fetch external stylesheet from url ...
    Css::Stylesheet* sub_stylesheet = LoadStylesheet(url);
    if (sub_stylesheet == NULL) {
      driver_->ErrorHere("Failed to load sub-resource %s",
                             url.as_string().c_str());
      return false;
    }

    // ... and recursively add all its sub-stylesheets (and it) to vector.
    if (!LoadAllSubStylesheets(sub_stylesheet, all_stylesheets)) {
      return false;
    }
  }
  // Add base stylesheet after all imports have been added.
  all_stylesheets->push_back(base_stylesheet);
  return true;
}


// Read an external CSS file, rewrite it and write a new external CSS file.
bool CssFilter::RewriteExternalCss(const StringPiece& in_url,
                                   GoogleString* out_url) {
  scoped_ptr<CachedResult> rewrite_info(RewriteWithCaching(in_url, NULL));
  if (rewrite_info.get() != NULL && rewrite_info->optimizable()) {
    *out_url = rewrite_info->url();
    return true;
  }
  return false;
}

RewriteSingleResourceFilter::RewriteResult CssFilter::RewriteLoadedResource(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  CHECK(input_resource->loaded());
  bool ret = false;
  if (input_resource->ContentsValid()) {
    // Rewrite stylesheet.
    StringPiece in_contents = input_resource->contents();
    GoogleString out_contents;
    // TODO(sligocki): Store the GURL in the input_resource.
    GoogleUrl css_gurl(input_resource->url());
    if (css_gurl.is_valid()) {
      TimedBool result = RewriteCssText(
          NULL /* no context*/, css_gurl, in_contents, &out_contents,
          driver_->message_handler());
      if (result.value) {
        // Write new stylesheet.
        int64 expire_ms = std::min(result.expiration_ms,
                                   input_resource->CacheExpirationTimeMs());
        output_resource->SetType(&kContentTypeCss);
        if (resource_manager_->Write(HttpStatus::kOK,
                                     out_contents,
                                     output_resource.get(),
                                     expire_ms,
                                     driver_->message_handler())) {
          ret = output_resource->IsWritten();
        }
      }
    }
  }
  return ret ? kRewriteOk : kRewriteFailed;
}

bool CssFilter::HasAsyncFlow() const {
  return driver_->asynchronous_rewrites();
}

CssFilter::Context* CssFilter::MakeContext() {
  return new Context(this, driver_, cache_extender_,
                     image_rewrite_filter_, image_combiner_);
}

RewriteContext* CssFilter::MakeRewriteContext() {
  return MakeContext();
}

}  // namespace net_instaweb
