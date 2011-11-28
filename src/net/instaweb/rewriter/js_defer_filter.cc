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

// Author: atulvasu@google.com (Atul Vasu)

#include "net/instaweb/rewriter/public/js_defer_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

extern const char* JS_js_defer;

// TODO(atulvasu): Minify this script if minify is turned on.
const char* JsDeferFilter::kDeferJsCode = JS_js_defer;

JsDeferFilter::JsDeferFilter(HtmlParse* html_parse)
    : html_parse_(html_parse),
      script_in_progress_(NULL),
      script_src_(NULL),
      script_tag_scanner_(html_parse) {
}

JsDeferFilter::~JsDeferFilter() { }

void JsDeferFilter::StartDocument() {
  defer_js_ = StrCat(kDeferJsCode, "\n",
                     "pagespeed.deferInit();\n"
                     "pagespeed.addOnload(window, function() {\n"
                     "  pagespeed.deferJs.run();\n"
                     "});\n");
}

void JsDeferFilter::StartElement(HtmlElement* element) {
  if (script_in_progress_ != NULL) {
    html_parse_->ErrorHere("Before script closing, another element found");
    return;
  }

  switch (script_tag_scanner_.ParseScriptElement(element, &script_src_)) {
    case ScriptTagScanner::kJavaScript:
      script_in_progress_ = element;
      if (script_src_ != NULL) {
        html_parse_->InfoHere("Found script with src %s", script_src_->value());
      }
      break;
    case ScriptTagScanner::kUnknownScript: {
      GoogleString script_dump;
      element->ToString(&script_dump);
      html_parse_->InfoHere("Unrecognized script:'%s'", script_dump.c_str());
      break;
    }
    case ScriptTagScanner::kNonScript:
      break;
  }
}

void JsDeferFilter::Characters(HtmlCharactersNode* characters) {
  if (script_in_progress_ != NULL) {
    // Note that we're keeping a vector of nodes here,
    // and appending them lazily at the end.  This is
    // because there's usually only 1 HtmlCharactersNode involved,
    // and we end up not actually needing to copy the string.
    buffer_.push_back(characters);
  }
}

// Flatten script fragments in buffer_, using script_buffer to hold
// the data if necessary.  Return a StringPiece referring to the data.
StringPiece JsDeferFilter::FlattenBuffer(GoogleString* script_buffer) {
  const int buffer_size = buffer_.size();
  if (buffer_.size() == 1) {
    StringPiece result(buffer_[0]->contents());
    return result;
  } else {
    for (int i = 0; i < buffer_size; i++) {
      script_buffer->append(buffer_[i]->contents());
    }
    StringPiece result(*script_buffer);
    return result;
  }
}

void JsDeferFilter::AddDeferJsFunc(const StringPiece& func,
                                   const StringPiece& arg) {
  GoogleString escaped_arg;
  JavascriptCodeBlock::ToJsStringLiteral(arg, &escaped_arg);
  StrAppend(&defer_js_, func, "(", escaped_arg, ");\n");
}

void JsDeferFilter::RewriteInlineScript() {
  html_parse_->DeleteElement(script_in_progress_);
  const int buffer_size = buffer_.size();
  if (buffer_size > 0) {
    // First buffer up script data and wrap it around defer function.
    GoogleString script_buffer;
    AddDeferJsFunc("pagespeed.deferJs.addStr", FlattenBuffer(&script_buffer));
  }
}

// External script; replace with a function call to defer this url.
void JsDeferFilter::RewriteExternalScript() {
  html_parse_->DeleteElement(script_in_progress_);
  AddDeferJsFunc("pagespeed.deferJs.addUrl", script_src_->value());
}

// Reset state at end of script.
void JsDeferFilter::CompleteScriptInProgress() {
  buffer_.clear();
  script_in_progress_ = NULL;
  script_src_ = NULL;
}

void JsDeferFilter::EndElement(HtmlElement* element) {
  if (script_in_progress_ != NULL &&
      html_parse_->IsRewritable(script_in_progress_) &&
      html_parse_->IsRewritable(element)) {
    if (element->keyword() == HtmlName::kScript) {
      // TODO(atulvasu): Do scripts have both src and inline script?
      if (script_src_ == NULL) {
        RewriteInlineScript();
      } else {
        RewriteExternalScript();
      }
      CompleteScriptInProgress();
    } else {
      // Should not happen by construction (parser should not have tags here).
      if (script_in_progress_ == NULL) {
        html_parse_->ErrorHere("Non script close before script close.");
        return;
      }
    }
  } else if (element->keyword() == HtmlName::kBody) {
    // TODO(atulvasu): Move into EndDocument()
    if (html_parse_->IsRewritable(element)) {
      HtmlElement* script_node =
          html_parse_->NewElement(element, HtmlName::kScript);
      html_parse_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
      HtmlNode* script_code =
          html_parse_->NewCharactersNode(script_node, defer_js_);
      html_parse_->InsertElementBeforeCurrent(script_node);
      html_parse_->AppendChild(script_node, script_code);
      // No setup needed for next body tag.
      defer_js_ = "";
    } else {
      html_parse_->WarningHere("BODY tag got flushed, can't edit.");
    }
  }
}

void JsDeferFilter::EndDocument() {
  if (!defer_js_.empty()) {
    // Scripts never get executed if this happen.
    html_parse_->ErrorHere("BODY tag didn't close after last script");
    // TODO(atulvasu): Try to write here.
  }
}

void JsDeferFilter::Flush() {
  if (script_in_progress_ != NULL) {
    // This is wrong, because now this script will break, because it
    // could not be rewritten.
    html_parse_->InfoHere("Flush in mid-script; could not defer.");
    CompleteScriptInProgress();
  }
}

}  // namespace net_instaweb
