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

#include <algorithm>

#include "base/logging.h"
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
#ifndef NDEBUG
  CharStarCompareSensitive less_than;
  for (int i = 0, n = arraysize(javascript_mimetypes); i < n - 1; ++i) {
    DCHECK(less_than(javascript_mimetypes[i], javascript_mimetypes[i + 1]));
  }
#endif
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
  bool check_lang_attr = false;
  if (type_attr == NULL) {
    check_lang_attr = true;
  } else {
    bool decoding_error;
    StringPiece type_str = type_attr->DecodedValue(&decoding_error);
    if (decoding_error) {
      lang = kUnknownScript;                 // e.g. <script type=&#257;>
    } else if (type_str.data() == NULL) {    // e.g. <script type>
      // If the type attribute is empty (no =) then fall back to the lang attr.
      check_lang_attr = true;
    } else if (type_str.empty() || IsJsMime(Normalized(type_str))) {
      // An empty type string (but not whitespace-only!) is JS,
      // So is one that's a known mimetype once lowercased and
      // having its leading and trailing whitespace removed
      lang = kJavaScript;
    } else {
      lang = kUnknownScript;
    }
  }

  // Without type= the ultra-deprecated language attribute determines
  // things.  empty or null one is ignored. The test is done
  // case-insensitively, but leading/trailing whitespace matters.
  // (Note: null check on ->DecodedValueOrNull() above as it's passed
  // to GoogleString)
  if (check_lang_attr) {
    HtmlElement::Attribute* lang_attr = element->FindAttribute(
        HtmlName::kLanguage);

    if (lang_attr != NULL) {
      bool decoding_error;
      StringPiece lang_piece = lang_attr->DecodedValue(&decoding_error);
      if (decoding_error) {
        lang = kUnknownScript;                 // e.g. <script language=&#257;>
      } else if (lang_piece.data() == NULL) {
        lang = kJavaScript;                    // e.g. <script language>
      } else {
        GoogleString lang_str;
        lang_piece.CopyToString(&lang_str);
        LowerString(&lang_str);
        if (lang_str.empty() || IsJsMime(StrCat("text/", lang_str))) {
          lang = kJavaScript;
        } else {
          lang = kUnknownScript;
        }
      }
    } else {
      // JS is the default if nothing is specified at all.
      lang = kJavaScript;
    }
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
    if (Normalized(for_attr->DecodedValueOrNull()) != "window") {
      flags |= kExecuteForEvent;
    }
    GoogleString event_str = Normalized(event_attr->DecodedValueOrNull());
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
  CharStarCompareSensitive less_than;  // case-folding done by caller.
  return std::binary_search(
      javascript_mimetypes,
      javascript_mimetypes + arraysize(javascript_mimetypes),
      type_str.c_str(),
      less_than);
}

}  // namespace net_instaweb
