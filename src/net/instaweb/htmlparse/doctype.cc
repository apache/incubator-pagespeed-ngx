// Copyright 2010 Google Inc.
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
//
// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/htmlparse/public/doctype.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const DocType DocType::kUnknown(DocType::UNKNOWN);
const DocType DocType::kHTML5(DocType::HTML_5);
const DocType DocType::kXHTML5(DocType::XHTML_5);
const DocType DocType::kHTML4Strict(DocType::HTML_4_STRICT);
const DocType DocType::kHTML4Transitional(DocType::HTML_4_TRANSITIONAL);
const DocType DocType::kXHTML11(DocType::XHTML_1_1);
const DocType DocType::kXHTML10Strict(DocType::XHTML_1_0_STRICT);
const DocType DocType::kXHTML10Transitional(DocType::XHTML_1_0_TRANSITIONAL);

bool DocType::IsXhtml() const {
  switch (doctype_) {
    case XHTML_5:
    case XHTML_1_1:
    case XHTML_1_0_STRICT:
    case XHTML_1_0_TRANSITIONAL:
      return true;
    default:
      return false;
  }
}

bool DocType::IsVersion5() const {
  switch (doctype_) {
    case HTML_5:
    case XHTML_5:
      return true;
    default:
      return false;
  }
}

bool DocType::Parse(const StringPiece& directive,
                    const ContentType& content_type) {
  // Check if this is a doctype directive; don't bother parsing if it isn't.
  if (!StringCaseStartsWith(directive, "doctype ")) {
    return false;
  }

  // Parse the directive.
  std::vector<GoogleString> parts;
  ParseShellLikeString(directive, &parts);

  // Sanity check:
  DCHECK_LE(1U, parts.size());
  DCHECK(StringCaseEqual(parts[0], "doctype"));

  // Check for known doctypes.
  // See http://en.wikipedia.org/wiki/DOCTYPE
  doctype_ = UNKNOWN;
  if (parts.size() >= 2 && StringCaseEqual(parts[1], "html")) {
    if (parts.size() == 2) {
      if (content_type.IsXmlLike()) {
        doctype_ = XHTML_5;
      } else {
        doctype_ = HTML_5;
      }
    } else if (parts.size() == 5 && StringCaseEqual(parts[2], "public")) {
      if (parts[3] == "-//W3C//DTD HTML 4.01//EN") {
        doctype_ = HTML_4_STRICT;
      } else if (parts[3] == "-//W3C//DTD HTML 4.01 Transitional//EN") {
        doctype_ = HTML_4_TRANSITIONAL;
      } else if (parts[3] == "-//W3C//DTD XHTML 1.1//EN") {
        doctype_ = XHTML_1_1;
      } else if (parts[3] == "-//W3C//DTD XHTML 1.0 Strict//EN") {
        doctype_ = XHTML_1_0_STRICT;
      } else if (parts[3] == "-//W3C//DTD XHTML 1.0 Transitional//EN") {
        doctype_ = XHTML_1_0_TRANSITIONAL;
      }
    }
  }
  return true;
}

}  // net_instaweb
