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

#include "net/instaweb/rewriter/public/css_tag_scanner.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/writer.h"

namespace {
const char kTextCss[] = "text/css";
}

namespace net_instaweb {

const char CssTagScanner::kStylesheet[] = "stylesheet";

// Finds CSS files and calls another filter.
CssTagScanner::CssTagScanner(HtmlParse* html_parse) {
}

bool CssTagScanner::ParseCssElement(
    HtmlElement* element, HtmlElement::Attribute** href, const char** media) {
  int num_required_attributes_found = 0;
  *media = "";
  *href = NULL;
  if (element->keyword() == HtmlName::kLink) {
    // We must have all attributes rel='stylesheet' href='name.css', and
    // type='text/css', although they can be in any order.  If there are,
    // other attributes, we better learn about them so we don't lose them
    // in css_combine_filter.cc.
    int num_attrs = element->attribute_size();

    // 'media=' is optional, but our filter requires href=*, and rel=stylesheet,
    // and type=text/css.
    //
    // type should be "text/css", but if it's omitted, that's OK.
    //
    // TODO(jmarantz): Consider recognizing a wider variety of CSS references,
    // including inline css so that the outline_filter can use it.
    if ((num_attrs >= 2) || (num_attrs <= 4)) {
      for (int i = 0; i < num_attrs; ++i) {
        HtmlElement::Attribute& attr = element->attribute(i);
        if (attr.keyword() == HtmlName::kHref) {
          *href = &attr;
          ++num_required_attributes_found;
        } else if (attr.keyword() == HtmlName::kRel) {
          if (StringCaseEqual(attr.value(), kStylesheet)) {
            ++num_required_attributes_found;
          } else {
            // rel=something_else.  abort.
            num_required_attributes_found = 0;
            break;
          }
        } else if (attr.keyword() == HtmlName::kMedia) {
          *media = attr.value();
        } else {
          // The only other attribute we should see is type=text/css.  This
          // attribute is not required, but if the attribute we are
          // finding here is anything else then abort.
          if ((attr.keyword() != HtmlName::kType) ||
              !StringCaseEqual(attr.value(), kTextCss)) {
            num_required_attributes_found = 0;
            break;
          }
        }
      }
    }
  }

  // we require both 'href=...' and 'rel=stylesheet'.
  // TODO(jmarantz): warn when CSS elements aren't quite what we expect?
  return (num_required_attributes_found >= 2);
}

namespace {

bool ExtractQuote(GoogleString* url, char* quote) {
  bool ret = false;
  int size = url->size();
  if (size > 2) {
    *quote = (*url)[0];
    if (((*quote == '\'') || (*quote == '"')) && (*quote == (*url)[size - 1])) {
      ret = true;
      *url = url->substr(1, size - 2);
    }
  }
  return ret;
}

}  // namespace

// TODO(jmarantz): replace this scan-and-replace-in-one-shot methdology with
// a proper scanner/parser/filtering mechanism akin to HtmlParse/HtmlLexer.
// See http://www.w3.org/Style/CSS/SAC/ for the C Parser.
//
// TODO(jmarantz): Add parsing & absolutification of @import.
bool CssTagScanner::AbsolutifyUrls(
    const StringPiece& contents, const StringPiece& base_url,
    Writer* writer, MessageHandler* handler) {
  size_t pos = 0;
  size_t prev_pos = 0;
  bool ok = true;

  // If the CSS url was specified with an absolute path, use that to
  // absolutify any URLs referenced in the CSS text.
  //
  // TODO(jmarantz): Consider pasting in any CSS resources found in an import
  // statement, rather than merely absolutifying in the references.  This would
  // require a few changes in this class API.
  //
  // TODO(jmarantz): Consider calling image optimization, if enabled, on any
  // images found.
  GoogleUrl base_gurl(base_url);
  if (base_gurl.is_valid()) {
    while (ok && ((pos = contents.find("url(", pos)) != StringPiece::npos)) {
      ok = writer->Write(contents.substr(prev_pos, pos - prev_pos), handler);
      prev_pos = pos;
      pos += 4;
      size_t end_of_url = contents.find(')', pos);
      if ((end_of_url != StringPiece::npos) && (end_of_url != pos)) {
        GoogleString url;
        TrimWhitespace(contents.substr(pos, end_of_url - pos), &url);
        char quote;
        bool is_quoted = ExtractQuote(&url, &quote);
        GoogleUrl gurl(url);

        // Relative paths are considered invalid by GURL, and those are the
        // ones we need to resolve.
        if (!gurl.is_valid()) {
          GoogleUrl resolved(base_gurl, url);
          if (resolved.is_valid()) {
            ok = writer->Write("url(", handler);
            if (is_quoted) {
              writer->Write(StringPiece(&quote, 1), handler);
            }
            ok = writer->Write(resolved.Spec(), handler);
            if (is_quoted) {
              writer->Write(StringPiece(&quote, 1), handler);
            }
            ok = writer->Write(")", handler);
            prev_pos = end_of_url + 1;
          } else {
            int line = 1;
            for (size_t i = 0; i < pos; ++i) {
              line += (contents[i] == '\n');
            }
            handler->Error(
                base_url.as_string().c_str(), line,
                "CSS URL resolution failed: %s",
                url.c_str());
          }
        }
      }
    }
  }
  if (ok) {
    ok = writer->Write(contents.substr(prev_pos), handler);
  }
  return ok;
}

bool CssTagScanner::HasImport(const StringPiece& contents,
                              MessageHandler* handler) {
  // Search for case insensitive @import.
  size_t pos = -1;  // So that pos + 1 == 0 below.
  const StringPiece kImport("import");
  while ((pos = contents.find("@", pos + 1)) != StringPiece::npos) {
    // Rest is everything past the @ (non-inclusive).
    StringPiece rest = contents.substr(pos + 1);
    if (StringCaseStartsWith(rest, kImport)) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
