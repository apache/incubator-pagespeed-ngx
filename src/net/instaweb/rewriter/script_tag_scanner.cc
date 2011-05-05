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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/script_tag_scanner.h"

#include <set>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// The list is from HTML5, "4.3.1.1 Scripting languages"
class HtmlParse;

static const char* const javascript_mimetypes[] = {
  "application/ecmascript",
  "application/javascript",
  "application/x-ecmascript",
  "application/x-javascript",
  "text/ecmascript",
  "text/javascript",
  "text/javascript1.0",
  "text/javascript1.1",
  "text/javascript1.2",
  "text/javascript1.3",
  "text/javascript1.4",
  "text/javascript1.5",
  "text/jscript",
  "text/livescript",
  "text/x-ecmascript",
  "text/x-javascript"
};

ScriptTagScanner::ScriptTagScanner(HtmlParse* html_parse) {
  for (int i = 0; i < static_cast<int>(arraysize(javascript_mimetypes)); ++i) {
    javascript_mimetypes_.insert(GoogleString(javascript_mimetypes[i]));
  }
}

ScriptTagScanner::ScriptClassification ScriptTagScanner::ParseScriptElement(
    HtmlElement* element, HtmlElement::Attribute** src) {
  if (element->keyword() != HtmlName::kScript) {
    return kNonScript;
  }

  *src = element->FindAttribute(HtmlName::kSrc);

  // Figure out if we have JS or not.
  // This is based on the 'type' and 'language' attributes, with
  // type having precedence. For this determination, however,
  // <script type> acts as if the type attribute is not there,
  // which is different from <script type="">
  ScriptClassification lang;
  HtmlElement::Attribute* type_attr = element->FindAttribute(HtmlName::kType);
  HtmlElement::Attribute* lang_attr = element->FindAttribute(
      HtmlName::kLanguage);
  if (type_attr != NULL && type_attr->value() != NULL) {
    StringPiece type_str = type_attr->value();
    if (type_str.empty() || IsJsMime(Normalized(type_str))) {
      // An empty type string (but not whitespace-only!) is JS,
      // So is one that's a known mimetype once lowercased and
      // having its leading and trailing whitespace removed
      lang = kJavaScript;
    } else {
      lang = kUnknownScript;
    }
  } else if (lang_attr != NULL && lang_attr->value() != NULL) {
    // Without type= the ultra-deprecated language attribute determines things.
    // empty or null one is ignored. The test is done case-insensitively,
    // but leading/trailing whitespace matters.
    // (Note: null check on ->value() above as it's passed to GoogleString)
    GoogleString lang_str = lang_attr->value();
    LowerString(&lang_str);
    if (lang_str.empty() || IsJsMime(StrCat("text/", lang_str))) {
      lang = kJavaScript;
    } else {
      lang = kUnknownScript;
    }
  } else {
    // JS is the default if nothing is specified at all.
    lang = kJavaScript;
  }

  return lang;
}


int ScriptTagScanner::ExecutionMode(const HtmlElement* element) const {
  int flags = 0;

  if (element->FindAttribute(HtmlName::kAsync) != NULL) {
    flags |= kExecuteAsync;
  }

  if (element->FindAttribute(HtmlName::kDefer) != NULL) {
    flags |= kExecuteDefer;
  }

  // HTML5 notes that certain values of IE-proprietary 'for' and 'event'
  // attributes are magic and are to be handled as if they're not there,
  // while others will cause the script to not be run at all.
  // Note: there is a disagreement between Chrome and Firefox on how
  // empty ones are handled. We set kExecuteForEvent as it is the conservative
  // value, requiring careful treatment by filters
  const HtmlElement::Attribute* for_attr = element->FindAttribute(
      HtmlName::kFor);
  const HtmlElement::Attribute* event_attr = element->FindAttribute(
      HtmlName::kEvent);
  if (for_attr != NULL && event_attr != NULL) {
    if (Normalized(for_attr->value()) != "window") {
      flags |= kExecuteForEvent;
    }
    GoogleString event_str = Normalized(event_attr->value());
    if (event_str != "onload" && event_str != "onload()") {
      flags |= kExecuteForEvent;
    }
  }

  return flags;
}

GoogleString ScriptTagScanner::Normalized(const StringPiece& str) {
  GoogleString normal_form;
  TrimWhitespace(str, &normal_form);
  LowerString(&normal_form);
  return normal_form;
}

bool ScriptTagScanner::IsJsMime(const GoogleString& type_str) {
  return javascript_mimetypes_.find(type_str) != javascript_mimetypes_.end();
}

}  // namespace net_instaweb
