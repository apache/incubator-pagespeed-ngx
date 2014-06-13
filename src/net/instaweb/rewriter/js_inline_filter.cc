// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/js_inline_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/inline_rewrite_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

const char JsInlineFilter::kNumJsInlined[] = "num_js_inlined";

class JsInlineFilter::Context : public InlineRewriteContext {
 public:
  Context(JsInlineFilter* filter, HtmlElement* element,
          HtmlElement::Attribute* src)
      : InlineRewriteContext(filter, element, src), filter_(filter) {}

  virtual bool ShouldInline(const ResourcePtr& resource,
                            GoogleString* reason) const {
    return filter_->ShouldInline(resource, reason);
  }

  virtual void RenderInline(
      const ResourcePtr& resource, const StringPiece& text,
      HtmlElement* element) {
    filter_->RenderInline(resource, text, element);
  }

  virtual const char* id() const { return RewriteOptions::kJavascriptInlineId; }

 private:
  JsInlineFilter* filter_;
  DISALLOW_COPY_AND_ASSIGN(Context);
};

JsInlineFilter::JsInlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      size_threshold_bytes_(driver->options()->js_inline_max_bytes()),
      script_tag_scanner_(driver),
      should_inline_(false) {
  Statistics* stats = server_context()->statistics();
  num_js_inlined_ = stats->GetVariable(kNumJsInlined);
}

JsInlineFilter::~JsInlineFilter() {}

void JsInlineFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumJsInlined);
}

void JsInlineFilter::StartDocumentImpl() {
  should_inline_ = false;
}

void JsInlineFilter::EndDocument() {
}

void JsInlineFilter::StartElementImpl(HtmlElement* element) {
  DCHECK(!should_inline_);

  HtmlElement::Attribute* src;
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    should_inline_ = (src != NULL) && (src->DecodedValueOrNull() != NULL);
  }
}

void JsInlineFilter::EndElementImpl(HtmlElement* element) {
  if (should_inline_ && driver()->IsRewritable(element)) {
    DCHECK(element->keyword() == HtmlName::kScript);
    HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kSrc);
    CHECK(attr != NULL);
    const char* src = attr->DecodedValueOrNull();
    DCHECK(src != NULL) << "should_inline_ should be false if attr val is null";

    // StartInlining() transfers ownership of ctx to RewriteDriver, or deletes
    // it on failure.
    // TODO(morlovich): Consider async/defer here; it may not be a good
    // idea to inline async scripts in particular.
    Context* ctx = new Context(this, element, attr);
    ctx->StartInlining();
  }
  should_inline_ = false;
}

bool JsInlineFilter::ShouldInline(const ResourcePtr& resource,
                                  GoogleString* reason) const {
  // Don't inline if it's too big or looks like it's trying to get at its own
  // url.

  StringPiece contents(resource->contents());
  if (contents.size() > size_threshold_bytes_) {
    *reason = StrCat("JS not inlined since it's bigger than ",
                     Integer64ToString(size_threshold_bytes_),
                     " bytes");
    return false;
  }

  if (driver()->options()->avoid_renaming_introspective_javascript() &&
      JavascriptCodeBlock::UnsafeToRename(contents)) {
    *reason = "JS not inlined since it may be looking for its source";
    return false;
  }

  return true;
}

void JsInlineFilter::RenderInline(
    const ResourcePtr& resource, const StringPiece& contents,
    HtmlElement* element) {
  // If it contains '</script' we need to escape.  The standard way to do this
  // is to replace </script with <\/script, but escaping / with \ is only valid
  // inside strings, and the following is legal javascript:
  //
  //   pathological.js:
  //     if(2</script>/) {
  //       alert("foo");
  //     } else {
  //       alert("bar");
  //     }
  //
  // This checks whether 2 is less than the regexp "/script>/".  While I would
  // be fine just abandoning this as too unlikely to worry about, we can
  // actually support this by encoding 's' as \x73 and using <\x73cript instead.
  // The html parser won't read that as </script> but the js parser will.
  //
  // Unfortunately escaping </script> can expose a different bug where browsers
  // treat <script> specially inside inline scripts after <!--.  So if we
  // currently have:
  //
  //   nested.js:
  //     <!--
  //     document.write("<script>...</script>");
  //
  // and we inline it as:
  //
  //   <script><!--
  //     document.write("<script>...</\x73cript>");
  //   </script>
  //
  // then the browser will treat the </script> tag as closing the <script>
  // that's inside the document.write, and will continue parsing the rest of the
  // document as javascript.  We were already open to this bug with code that
  // included <script> without </script> but that's probably less common.  So we
  // should escape <script> too.
  //
  // Because there are legitimate uses of "<script" where it's part of an
  // identifier we can't use the shorter \xNN notation but need \uNNNN notation
  // instead.  I don't know why they decided \uNNNN would be good for both
  // strings and identifiers but \xNN would be good only for strings, but that's
  // the way it is.  For clarity (and gzip?) we'll just use \uNNNN everywhere.
  GoogleString contents_for_escaping;
  StringPiece escaped_contents;
  // First quickly scan to see if there's anything we need to fix.
  if (FindIgnoreCase(contents, "<script") != StringPiece::npos ||
      FindIgnoreCase(contents, "</script") != StringPiece::npos) {
    contents.CopyToString(&contents_for_escaping);

    // To keep the case of the original 'script' text we need to run twice, once
    // for 's' and once for 'S'.
    RE2::GlobalReplace(&contents_for_escaping,
                       "<(/?)s([cC][rR][iI][pP][tT])",
                       "<\\1\\\\u0073\\2");
    RE2::GlobalReplace(&contents_for_escaping,
                       "<(/?)S([cC][rR][iI][pP][tT])",
                       "<\\1\\\\u0053\\2");

    escaped_contents = contents_for_escaping;
  } else {
    escaped_contents = contents;
  }

  // If we're in XHTML, we should wrap the script in a <!CDATA[...]]>
  // block to ensure that we don't break well-formedness.  Since XHTML is
  // sometimes interpreted as HTML (which will ignore CDATA delimiters),
  // we have to hide the CDATA delimiters behind Javascript comments.
  // See http://lachy.id.au/log/2006/11/xhtml-script
  // and http://code.google.com/p/modpagespeed/issues/detail?id=125
  if (driver()->MimeTypeXhtmlStatus() != RewriteDriver::kIsNotXhtml) {
    // CDATA sections cannot be nested because they end with the first
    // occurrence of "]]>", so if the script contains that string
    // anywhere (and we're in XHTML) we can't inline.
    // TODO(mdsteele): We should consider escaping somehow.
    if (escaped_contents.find("]]>") == StringPiece::npos) {
      HtmlCharactersNode* node =
          driver()->NewCharactersNode(element, "//<![CDATA[\n");
      node->Append(escaped_contents);
      node->Append("\n//]]>");
      driver()->AppendChild(element, node);
      element->DeleteAttribute(HtmlName::kSrc);
    }
  } else {
    // If we're not in XHTML, we can simply paste in the external script
    // verbatim.
    driver()->AppendChild(
        element, driver()->NewCharactersNode(element, escaped_contents));
    element->DeleteAttribute(HtmlName::kSrc);
  }
  num_js_inlined_->Add(1);
}

void JsInlineFilter::Characters(HtmlCharactersNode* characters) {
  if (should_inline_) {
    HtmlElement* script_element = characters->parent();
    DCHECK(script_element != NULL);
    DCHECK_EQ(HtmlName::kScript, script_element->keyword());
    if (driver()->IsRewritable(script_element) &&
        OnlyWhitespace(characters->contents())) {
      // If it's just whitespace inside the script tag, it's (probably) safe to
      // just remove it.
      driver()->DeleteNode(characters);
    } else {
      // This script tag isn't empty, despite having a src field.  The contents
      // won't be executed by the browser, but will still be in the DOM; some
      // external scripts like to use this as a place to store data.  So, we'd
      // better not try to inline in this case.
      should_inline_ = false;
    }
  }
}

}  // namespace net_instaweb
