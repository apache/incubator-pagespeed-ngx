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
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
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

}  // namespace

// Statistics variable names.
const char CssFilter::kFilesMinified[] = "css_filter_files_minified";
const char CssFilter::kMinifiedBytesSaved[] = "css_filter_minified_bytes_saved";
const char CssFilter::kParseFailures[] = "css_filter_parse_failures";

class CssFilter::Context : public SingleRewriteContext {
 public:
  Context(CssFilter* filter, RewriteDriver* driver)
      : SingleRewriteContext(driver, NULL /* no parent */,
                             NULL /* no resource context */),
        filter_(filter),
        driver_(driver) {}
  virtual ~Context() {}

  virtual void Render() {}
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id().c_str(); }
  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }

 private:
  CssFilter* filter_;
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

void CssFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  RewriteDone(
      filter_->RewriteLoadedResource(input_resource, output_resource), 0);
}

CssFilter::CssFilter(RewriteDriver* driver, const StringPiece& path_prefix,
                     CacheExtender* cache_extender,
                     ImageRewriteFilter* image_rewriter,
                     ImageCombineFilter* image_combiner)
    : RewriteSingleResourceFilter(driver, path_prefix),
      in_style_element_(false),
      image_rewriter_(driver, cache_extender, image_rewriter, image_combiner),
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
      if (RewriteCssText(style_char_node_->contents(), &new_content,
                         driver_->base_url(),
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
          ResourcePtr input_resource(
              CreateInputResource(element_href->value()));
          if (input_resource.get() != NULL) {
            ResourceSlotPtr slot(
                driver_->GetSlot(input_resource, element, element_href));
            Context* context = new Context(this, driver_);
            context->AddSlot(slot);
            driver_->InitiateRewrite(context);
          }
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
TimedBool CssFilter::RewriteCssText(const StringPiece& in_text,
                               GoogleString* out_text,
                               const GoogleUrl& css_gurl,
                               MessageHandler* handler) {
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
    TimedBool result;
    if (HasAsyncFlow()) {
      // TODO(morlovich): Nested rewrites disabled for async flow as first step!
      result.value = false;
    } else {
      result = image_rewriter_.RewriteCssImages(
          css_gurl, stylesheet.get(), handler);
      ret.expiration_ms = result.expiration_ms;
    }

    // Re-serialize stylesheet.
    StringWriter writer(out_text);
    CssMinify::Stylesheet(*stylesheet, &writer, handler);

    // Get signed versions so that we can subtract them.
    int64 out_text_size = static_cast<int64>(out_text->size());
    int64 in_text_size = static_cast<int64>(in_text.size());
    int64 bytes_saved = in_text_size - out_text_size;

    if (!driver_->options()->always_rewrite_css()) {
      // Don't rewrite if we didn't edit it or make it any smaller.
      if (!result.value && bytes_saved <= 0) {
        ret.value = false;
        driver_->InfoHere("CSS parser increased size of CSS file %s by %lld "
                          "bytes.", css_gurl.spec_c_str(),
                          static_cast<long long int>(-bytes_saved));
      }
      // Don't rewrite if we blanked the CSS file! (This is a parse error)
      // TODO(sligocki): Don't error if in_text is all whitespace.
      if (out_text_size == 0 && in_text_size != 0) {
        ret.value = false;
        driver_->InfoHere("CSS parsing error in %s", css_gurl.spec_c_str());
        num_parse_failures_->Add(1);
      }
    }

    // Statistics
    if (ret.value) {
      driver_->InfoHere("Successfully rewrote CSS file %s saving %lld "
                        "bytes.", css_gurl.spec_c_str(),
                        static_cast<long long int>(bytes_saved));
      num_files_minified_->Add(1);
      minified_bytes_saved_->Add(bytes_saved);
    }
    // TODO(sligocki): Do we want to save the AST 'stylesheet' somewhere?
    // It currently, deletes itself at the end of the function.
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
      TimedBool result = RewriteCssText(in_contents, &out_contents, css_gurl,
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

RewriteContext* CssFilter::MakeRewriteContext() {
  return new Context(this, driver_);
}

}  // namespace net_instaweb
