/**
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

#include "base/at_exit.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/writer.h"
#include "webutil/css/parser.h"

namespace {

base::AtExitManager* at_exit_manager = NULL;

}  // namespace

namespace net_instaweb {

namespace {

const char kStylesheet[] = "stylesheet";

}  // namespace

// Statistics variable names.
const char CssFilter::kFilesMinified[] = "css_filter_files_minified";
const char CssFilter::kMinifiedBytesSaved[] = "css_filter_minified_bytes_saved";
const char CssFilter::kParseFailures[] = "css_filter_parse_failures";

CssFilter::CssFilter(RewriteDriver* driver, const StringPiece& path_prefix)
    : RewriteSingleResourceFilter(driver, path_prefix),
      html_parse_(driver->html_parse()),
      resource_manager_(driver->resource_manager()),
      in_style_element_(false),
      s_style_(html_parse_->Intern("style")),
      s_link_(html_parse_->Intern("link")),
      s_rel_(html_parse_->Intern("rel")),
      s_href_(html_parse_->Intern("href")),
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

void CssFilter::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(CssFilter::kFilesMinified);
    statistics->AddVariable(CssFilter::kMinifiedBytesSaved);
    statistics->AddVariable(CssFilter::kParseFailures);
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
  if (element->tag() == s_style_) {
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
      html_parse_->ErrorHere("Multiple character nodes in style.");
      in_style_element_ = false;
    }
  }
}

void CssFilter::EndElementImpl(HtmlElement* element) {
  // Rewrite an inline style.
  if (in_style_element_) {
    CHECK(style_element_ == element);  // HtmlParse should not pass unmatching.

    if (html_parse_->IsRewritable(element) && style_char_node_ != NULL) {
      CHECK(element == style_char_node_->parent());  // Sanity check.
      std::string new_content;
      if (RewriteCssText(style_char_node_->contents(), &new_content,
                         StrCat("inline CSS in ", html_parse_->url()),
                         html_parse_->message_handler())) {
        // Note: Copy of new_content here.
        HtmlCharactersNode* new_style_char_node =
            html_parse_->NewCharactersNode(element, new_content);
        html_parse_->ReplaceNode(style_char_node_, new_style_char_node);
      }
    }
    in_style_element_ = false;

  // Rewrite an external style.
  } else if (element->tag() == s_link_ && html_parse_->IsRewritable(element)) {
    StringPiece relation(element->AttributeValue(s_rel_));
    if (relation == kStylesheet) {
      HtmlElement::Attribute* element_href = element->FindAttribute(s_href_);
      if (element_href != NULL) {
        // If it has a href= attribute
        std::string new_url;
        if (RewriteExternalCss(element_href->value(), &new_url)) {
          element_href->SetValue(new_url);  // Update the href= attribute.
        }
      } else {
        html_parse_->ErrorHere("Link element with no href.");
      }
    }
  }
}

// Return value answers the question: May we rewrite?
// If return false, out_text is undefined.
// id should be the URL for external CSS and other identifying info
// for inline CSS. It is used to log where the CSS parsing error was.
bool CssFilter::RewriteCssText(const StringPiece& in_text,
                               std::string* out_text,
                               const std::string& id,
                               MessageHandler* handler) {
  // Load stylesheet w/o expanding background attributes.
  Css::Parser parser(in_text);
  scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

  bool ret = false;
  if (parser.errors_seen_mask() != Css::Parser::kNoError) {
    html_parse_->InfoHere("CSS parsing error in %s", id.c_str());
    if (num_parse_failures_ != NULL) {
      num_parse_failures_->Add(1);
    }
  } else {
    // TODO(sligocki): Edit stylesheet.

    // Re-serialize stylesheet.
    StringWriter writer(out_text);
    CssMinify::Stylesheet(*stylesheet, &writer, handler);

    // Get signed versions so that we can subtract them.
    int64 out_text_size = static_cast<int64>(out_text->size());
    int64 in_text_size = static_cast<int64>(in_text.size());

    // Don't rewrite if we don't make it smaller.
    ret = (out_text_size < in_text_size);

    // Don't rewrite if we blanked the CSS file! (This is a parse error)
    // TODO(sligocki): Don't error if in_text is all whitespace.
    if (out_text_size == 0 && in_text_size != 0) {
      ret = false;
      html_parse_->InfoHere("CSS parsing error in %s", id.c_str());
      if (num_parse_failures_ != NULL) {
        num_parse_failures_->Add(1);
      }
    }

    // Statistics
    if (ret && num_files_minified_ != NULL) {
      num_files_minified_->Add(1);
      minified_bytes_saved_->Add(in_text_size - out_text_size);
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
      html_parse_->ErrorHere("Failed to load sub-resource %s",
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
                                   std::string* out_url) {
  bool ret = false;
  scoped_ptr<Resource> input_resource(CreateInputResource(in_url));
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceFromResource(
          filter_prefix_, &kContentTypeCss, resource_manager_->url_escaper(),
          input_resource.get(), html_parse_->message_handler()));
  if (output_resource.get() != NULL &&
      RewriteExternalCssToResource(input_resource.get(),
                                   output_resource.get())) {
    ret = true;
    *out_url = output_resource->url();
  }
  return ret;
}

// Rewrite in input_resource once it has already been loaded.
bool CssFilter::RewriteExternalCssToResource(Resource* input_resource,
                                             OutputResource* output_resource) {
  bool ret = false;
  // If this OutputResource has not already been created, create it.
  if (!output_resource->IsWritten()) {
    // Load input stylesheet.
    MessageHandler* handler = html_parse_->message_handler();
    if (input_resource != NULL &&
        resource_manager_->ReadIfCached(input_resource, handler)) {
      if (input_resource->ContentsValid()) {
        ret = RewriteLoadedResource(input_resource, output_resource);
      } else {
        // TODO(sligocki): Should these really be HtmlParse warnings?
        html_parse_->WarningHere("CSS resource fetch failed: %s",
                                 input_resource->url().c_str());
      }
    }
  }

  return ret;
}

bool CssFilter::RewriteLoadedResource(const Resource* input_resource,
                                      OutputResource* output_resource) {
  CHECK(input_resource->loaded());
  if (input_resource->ContentsValid()) {
    // Rewrite stylesheet.
    StringPiece in_contents = input_resource->contents();
    std::string out_contents;
    if (!RewriteCssText(in_contents, &out_contents, input_resource->url(),
                        html_parse_->message_handler())) {
      return false;
    }

    // Write new stylesheet.
    // TODO(sligocki): Set expire time.
    if (!resource_manager_->Write(HttpStatus::kOK, out_contents,
                                  output_resource, -1,
                                  html_parse_->message_handler())) {
      return false;
    }
  }

  return output_resource->IsWritten();
}

}  // namespace net_instaweb
