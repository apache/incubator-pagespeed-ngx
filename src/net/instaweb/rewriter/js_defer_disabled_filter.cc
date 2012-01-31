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

#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/util/public/null_message_handler.h"

namespace net_instaweb {

extern const char* JS_js_defer;  // Non-optimized output.


GoogleString* JsDeferDisabledFilter::opt_defer_js_ = NULL;
GoogleString* JsDeferDisabledFilter::debug_defer_js_ = NULL;

JsDeferDisabledFilter::JsDeferDisabledFilter(RewriteDriver* driver)
    : rewrite_driver_(driver),
      script_written_(false),
      defer_js_enabled_(false),
      debug_(false) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::StartDocument() {
  script_written_ = false;
  defer_js_enabled_ = rewrite_driver_->UserAgentSupportsJsDefer();
}

void JsDeferDisabledFilter::Initialize(Statistics* statistics) {
  static const char kSuffix[] =
      "\npagespeed.deferInit();\n"
      "pagespeed.deferJs.registerScriptTags();\n"
      "pagespeed.addOnload(window, function() {\n"
      "  pagespeed.deferJs.run();\n"
      "});\n";
  if (debug_defer_js_ == NULL) {
    debug_defer_js_ = new GoogleString(StrCat(JS_js_defer, kSuffix));
    JavascriptRewriteConfig config(NULL);
    NullMessageHandler handler;
    JavascriptCodeBlock code_block(*debug_defer_js_, &config, "init", &handler);
    bool ok = code_block.ProfitableToRewrite();
    DCHECK(ok);
    if (ok) {
      opt_defer_js_ = new GoogleString;
      code_block.Rewritten().CopyToString(opt_defer_js_);
    } else {
      opt_defer_js_ = debug_defer_js_;
    }
  }
}

void JsDeferDisabledFilter::Terminate() {
  if (opt_defer_js_ != debug_defer_js_) {
    delete opt_defer_js_;
  }
  delete debug_defer_js_;
  opt_defer_js_ = NULL;
  debug_defer_js_ = NULL;
}

void JsDeferDisabledFilter::EndElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kBody &&
      !script_written_) {
    HtmlElement* script_node =
        rewrite_driver_->NewElement(element, HtmlName::kScript);
    rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                              "text/javascript");
    const GoogleString& defer_js = debug_ ? *debug_defer_js_ : *opt_defer_js_;
    HtmlNode* script_code =
        rewrite_driver_->NewCharactersNode(script_node, defer_js);
    rewrite_driver_->InsertElementBeforeCurrent(script_node);
    rewrite_driver_->AppendChild(script_node, script_code);
    script_written_ = true;
  }
}

void JsDeferDisabledFilter::EndDocument() {
  if (defer_js_enabled_ && !script_written_) {
    // Scripts never get executed if this happen.
    rewrite_driver_->InfoHere("BODY tag didn't close after last script");
    // TODO(atulvasu): Try to write here.
  }
}

}  // namespace net_instaweb
