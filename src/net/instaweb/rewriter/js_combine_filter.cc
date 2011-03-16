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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/js_combine_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {
// This filter combines multiple external JS scripts into a single one, in order
// to reduce the amount of fetches that need to be done. The transformation is
// as follows:
//
// <script src="a.js">
// <stuff>
// <script src="b.js">
//
// gets turned into:
//
// <script src="a.js+b.js">
// <script>eval(mod_pagespeed_${hash("a.js")})</script>
// <stuff>
// <script>eval(mod_pagespeed_${hash("b.js")})</script>
//
// where $hash stands for using the active Hasher and tweaking the result to
// be a valid identifier continuation. Further, the combined source file
// has the code:
// var mod_pagespeed_${hash("a.js")} = "code of a.js as a string literal";
// var mod_pagespeed_${hash("b.js")} = "code of b.js as a string literal";

const char JsCombineFilter::kJsFileCountReduction[] = "js_file_count_reduction";

class JsCombineFilter::JsCombiner
    : public ResourceCombinerTemplate<HtmlElement*> {
 public:
  JsCombiner(JsCombineFilter* filter, RewriteDriver* driver,
             const StringPiece& filter_prefix)
      : ResourceCombinerTemplate<HtmlElement*>(
          driver, filter_prefix, kContentTypeJavascript.file_extension() + 1,
          filter),
        filter_(filter),
        js_file_count_reduction_(NULL) {
    filter_prefix.CopyToString(&filter_prefix_);
    Statistics* stats = resource_manager_->statistics();
    if (stats != NULL) {
      js_file_count_reduction_ = stats->GetVariable(kJsFileCountReduction);
    }
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // In strict mode of ES262-5 eval runs in a private variable scope,
    // (see 10.4.2 step 3 and 10.4.2.1), so our transformation is not safe.
    // Strict mode is identified by 'use strict' or "use strict" string literals
    // (escape-free) in some contexts. As a conservative approximation, we just
    // look for the text
    if (resource->contents().find("use strict") != StringPiece::npos) {
      return false;
    }

    // TODO(morlovich): TODO(...): define a pragma that javascript authors can
    // include in their source to prevent inclusion in a js combination
    return true;
  }

  // Try to combine all the JS files we have seen so far, modifying the
  // HTML if successful. Regardless of success or failure, the combination
  // will be empty after this call returns. If the last tag inside the
  // combination is currently open, it will be excluded from the combination.
  void TryCombineAccumulated();

 private:
  virtual bool WritePiece(Resource* input, OutputResource* combination,
                          Writer* writer, MessageHandler* handler);

  JsCombineFilter* filter_;
  std::string filter_prefix_;
  Variable* js_file_count_reduction_;
};

void JsCombineFilter::JsCombiner::TryCombineAccumulated() {
  if (num_urls() > 1) {
    MessageHandler* handler = rewrite_driver_->message_handler();

    // Since we explicitly disallow nesting, and combine before flushes,
    // the only potential problem is if we have an open script element
    // (with src) with the flush window happening before </script>.
    // In that case, we back it out from this combination.
    // This case also occurs if we're forced to give up on a script
    // element due to nested content and the like.
    HtmlElement* last = element(num_urls() - 1);
    if (!rewrite_driver_->IsRewritable(last)) {
      RemoveLastElement();
      if (num_urls() == 1) {
        // We ended up with only one thing in collection, so there is nothing
        // to do any more.
        Reset();
        return;
      }
    }

    // Make or reuse from cache the combination of the resources.
    scoped_ptr<OutputResource> combination(
        Combine(kContentTypeJavascript, handler));

    if (combination.get() != NULL) {
      // Now create an element for the combination, insert it before first one.
      HtmlElement* combine_element =
          rewrite_driver_->NewElement(NULL, HtmlName::kScript);
      rewrite_driver_->InsertElementBeforeElement(element(0), combine_element);

      rewrite_driver_->AddAttribute(combine_element, HtmlName::kSrc,
                                    combination->url());

      // Rewrite the scripts included in the combination to have as their bodies
      // eval() of variables including their code and no src.
      for (int i = 0; i < num_urls(); ++i) {
        HtmlElement* original = element(i);
        HtmlElement* modified = rewrite_driver_->CloneElement(original);
        modified->DeleteAttribute(HtmlName::kSrc);
        rewrite_driver_->InsertElementBeforeElement(original, modified);
        rewrite_driver_->DeleteElement(original);
        std::string var_name = filter_->VarName(resources()[i]->url());
        HtmlNode* script_code = rewrite_driver_->NewCharactersNode(
            modified, StrCat("eval(", var_name, ");"));
        rewrite_driver_->AppendChild(modified, script_code);
      }
    }

    rewrite_driver_->InfoHere("Combined %d JS files into one at %s",
                          num_urls(),
                          combination->url().c_str());
    if (js_file_count_reduction_ != NULL) {
      js_file_count_reduction_->Add(num_urls() - 1);
    }
  }
  Reset();
}

bool JsCombineFilter::JsCombiner::WritePiece(
    Resource* input, OutputResource* combination, Writer* writer,
    MessageHandler* handler) {
  // We write out code of each script into a variable.
  writer->Write(StrCat("var ", filter_->VarName(input->url()), " = \""),
                handler);

  // We escape backslash, double-quote, CR and LF while forming a string
  // from the code. This is /almost/ completely right: U+2028 and U+2029 are
  // line terminators as well (ECMA 262-5 --- 7.3, 7.8.4), so should really be
  // escaped, too, but we don't have the encoding here.
  StringPiece original = input->contents();
  std::string escaped;
  for (size_t c = 0; c < original.length(); ++c) {
    switch (original[c]) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped += original[c];
    }
  }

  writer->Write(escaped, handler);
  writer->Write("\";\n", handler);
  return true;
}

JsCombineFilter::JsCombineFilter(RewriteDriver* driver,
                                 const char* filter_prefix)
    : RewriteFilter(driver, filter_prefix),
      script_scanner_(driver),
      script_depth_(0),
      current_js_script_(NULL) {
  combiner_.reset(new JsCombiner(this, driver, filter_prefix));
}

JsCombineFilter::~JsCombineFilter() {
}

void JsCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kJsFileCountReduction);
}

void JsCombineFilter::StartDocumentImpl() {
}

void JsCombineFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* src;
  ScriptTagScanner::ScriptClassification classification =
      script_scanner_.ParseScriptElement(element, &src);
  switch (classification) {
    case ScriptTagScanner::kNonScript:
      if (script_depth_ > 0) {
        // We somehow got some tag inside a script. Be conservative ---
        // it may be meaningful so we don't want to destroy it;
        // so flush the complete things before us, and call it a day.
        if (IsCurrentScriptInCombination()) {
          combiner_->RemoveLastElement();
        }
        combiner_->TryCombineAccumulated();
      }
      break;

    case ScriptTagScanner::kJavaScript:
      ConsiderJsForCombination(element, src);
      ++script_depth_;
      break;

    case ScriptTagScanner::kUnknownScript:
      // We have something like vbscript. Handle this as a barrier
      combiner_->TryCombineAccumulated();
      ++script_depth_;
      break;
  }
}

void JsCombineFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kScript) {
    --script_depth_;
    if (script_depth_ == 0) {
      current_js_script_ = NULL;
    }
  }
}

void JsCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  combiner_->TryCombineAccumulated();
}

void JsCombineFilter::Characters(HtmlCharactersNode* characters) {
  // If a script has non-whitespace data inside of it, we cannot
  // replace its contents with a call to eval, as they may be needed.
  if (script_depth_ > 0 && !OnlyWhitespace(characters->contents())) {
    if (IsCurrentScriptInCombination()) {
      combiner_->RemoveLastElement();
      combiner_->TryCombineAccumulated();
    }
  }
}

void JsCombineFilter::Flush() {
  // We try to combine what we have thus far the moment we see a flush.
  // This serves two purposes:
  // 1) Let's us edit elements while they are still rewritable,
  //    but as late as possible.
  // 2) Ensures we do combine eventually (as we will get a flush at the end of
  //    parsing).
  combiner_->TryCombineAccumulated();
}

void JsCombineFilter::ConsiderJsForCombination(HtmlElement* element,
                                               HtmlElement::Attribute* src) {
  // Worst-case scenario is if we somehow ended up with nested scripts.
  // In this case, we just give up entirely.
  if (script_depth_ > 0) {
    driver_->WarningHere("Nested <script> elements");
    combiner_->Reset();
    return;
  }

  // Opening a new script normally...
  current_js_script_ = element;

  // Now we may have something that's not combinable; in those cases we would
  // like to flush as much as possible.
  // TODO(morlovich): if we stick with the current eval-based strategy, this
  // is way too conservative, as we keep multiple script elements for
  // actual execution.

  // If our current script may be inside a noscript, which means
  // we should not be making it runnable.
  if (noscript_element() != NULL) {
    combiner_->TryCombineAccumulated();
    return;
  }

  // An inline script.
  if (src == NULL || src->value() == NULL) {
    combiner_->TryCombineAccumulated();
    return;
  }

  // We do not try to merge in a <script with async/defer> or for/event.
  // TODO(morlovich): is it worth combining multiple scripts with
  // async/defer if the flags are the same?
  if (script_scanner_.ExecutionMode(element) != script_scanner_.kExecuteSync) {
    combiner_->TryCombineAccumulated();
    return;
  }

  // Now we see if policy permits us merging this element with previous ones.
  StringPiece url = src->value();
  MessageHandler* handler = driver_->message_handler();
  if (!combiner_->AddElement(element, src->value(), handler)) {
    // No -> try to flush what we have thus far.
    combiner_->TryCombineAccumulated();

    // ... and try to start a new combination
    combiner_->AddElement(element, src->value(), handler);
  }
}

bool JsCombineFilter::IsCurrentScriptInCombination() const {
  int included_urls = combiner_->num_urls();
  return (current_js_script_ != NULL) &&
         (included_urls >= 1) &&
         (combiner_->element(included_urls - 1) == current_js_script_);
}

std::string JsCombineFilter::VarName(const std::string& url) const {
  std::string url_hash = resource_manager_->hasher()->Hash(url);
  // Our hashes are web64, which are almost valid identifier continuations,
  // except for use of -. We simply replace it with $.
  std::size_t pos = 0;
  while ((pos = url_hash.find_first_of('-', pos)) != std::string::npos) {
    url_hash[pos] = '$';
  }

  return StrCat("mod_pagespeed_", url_hash);
}

bool JsCombineFilter::Fetch(OutputResource* resource,
                             Writer* writer,
                             const RequestHeaders& request_header,
                             ResponseHeaders* response_headers,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  return combiner_->Fetch(resource, writer, request_header, response_headers,
                          message_handler, callback);
}

}  // namespace net_instaweb
