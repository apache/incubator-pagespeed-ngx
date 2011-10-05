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

#include <cstddef>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace {
const char kTextCss[] = "text/css";
}

namespace net_instaweb {
class HtmlParse;

CssTagScanner::Transformer::~Transformer() {
}

const char CssTagScanner::kStylesheet[] = "stylesheet";
const char CssTagScanner::kUriValue[] = "url(";

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

// TODO(jmarantz): Add parsing & absolutification of @import.
bool CssTagScanner::TransformUrls(
    const StringPiece& contents, Writer* writer, Transformer* transformer,
    MessageHandler* handler) {
  size_t pos = 0;
  size_t prev_pos = 0;
  bool ok = true;

  // If the CSS url was specified with an absolute path, use that to
  // absolutify any URLs referenced in the CSS text.
  //
  // TODO(jmarantz): Consider calling image optimization, if enabled, on any
  // images found.
  while (ok && ((pos = contents.find(kUriValue, pos)) != StringPiece::npos)) {
    ok = writer->Write(contents.substr(prev_pos, pos - prev_pos), handler);
    prev_pos = pos;
    pos += 4;
    size_t end_of_url = contents.find(')', pos);
    GoogleString transformed;
    if ((end_of_url != StringPiece::npos) && (end_of_url != pos)) {
      GoogleString url;
      TrimWhitespace(contents.substr(pos, end_of_url - pos), &url);
      char quote;
      bool is_quoted = ExtractQuote(&url, &quote);
      if (transformer->Transform(url, &transformed)) {
        ok = writer->Write(kUriValue, handler);
        if (is_quoted) {
          writer->Write(StringPiece(&quote, 1), handler);
        }
        ok = writer->Write(transformed, handler);
        if (is_quoted) {
          writer->Write(StringPiece(&quote, 1), handler);
        }
        ok = writer->Write(")", handler);
        prev_pos = end_of_url + 1;
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

bool CssTagScanner::HasUrl(const StringPiece& contents) {
  return (contents.find(CssTagScanner::kUriValue) != StringPiece::npos);
}


RewriteDomainTransformer::RewriteDomainTransformer(
    const GoogleUrl* old_base_url, const GoogleUrl* new_base_url,
    RewriteDriver* driver)
    : old_base_url_(old_base_url), new_base_url_(new_base_url),
      domain_rewriter_(driver->domain_rewriter()),
      url_trim_filter_(driver->url_trim_filter()),
      handler_(driver->message_handler()) {
}

RewriteDomainTransformer::~RewriteDomainTransformer() {
}

bool RewriteDomainTransformer::Transform(const StringPiece& in,
                                         GoogleString* out) {
  GoogleString rewritten;
  if (domain_rewriter_->Rewrite(in, *old_base_url_, &rewritten)
      == DomainRewriteFilter::kFail) {
    return false;
  }
  // Note: Because of complications with sharding, we cannot trim
  // sharded resources against the final sharded domain of the CSS file.
  // Specifically, that final domain depends upon the precise text of that
  // we are altering here.
  if (!url_trim_filter_->Trim(*new_base_url_, rewritten, out, handler_)) {
    out->swap(rewritten);
  }
  return *out != in;
}

}  // namespace net_instaweb
