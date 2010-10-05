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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/url_left_trim_filter.h"

#include <vector>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

UrlLeftTrimFilter::UrlLeftTrimFilter(HtmlParse* html_parse,
                                     Statistics* stats)
    : html_parse_(html_parse),
      s_base_(html_parse->Intern("base")),
      s_href_(html_parse->Intern("href")),
      s_src_(html_parse->Intern("src")),
      trim_count_((stats == NULL) ? NULL : stats->AddVariable("url_trims")),
      trim_saved_bytes_(
          (stats == NULL) ? NULL : stats->AddVariable("url_trim_saved_bytes")) {
}

void UrlLeftTrimFilter::StartElement(HtmlElement* element) {
  // TODO(jmaessen): handle other places urls might lurk in html.
  // But never rewrite the base tag; always include its full url.
  if (element->tag() != s_base_) {
    TrimAttribute(element->FindAttribute(s_href_));
    TrimAttribute(element->FindAttribute(s_src_));
  }
}

void UrlLeftTrimFilter::AddTrimming(const StringPiece& trimming) {
  CHECK_LT(0u, trimming.size());
  left_trim_strings_.push_back(trimming.as_string());
}

void UrlLeftTrimFilter::AddBaseUrl(const StringPiece& base) {
  size_t colon_pos = base.find(':');
  size_t host_start = 0;
  if (colon_pos != base.npos) {
    StringPiece protocol(base.data(), colon_pos+1);
    AddTrimming(protocol);
    host_start = colon_pos + 3;
  } else {
    colon_pos = -1;
  }
  size_t first_slash_pos = base.find('/', host_start);
  if (first_slash_pos != base.npos) {
    StringPiece host_name(base.data() + colon_pos + 1,
                          first_slash_pos - colon_pos - 1);
    AddTrimming(host_name);
    size_t last_slash_pos = base.rfind('/');
    if (last_slash_pos != base.npos &&
        last_slash_pos > first_slash_pos) {
      // Note that we leave a case on the floor here: when base is the root of a
      // domain (such as http://www.nytimes.com/ ) we can strip the leading /
      // off rooted urls.  We do not do so as the path / is a proper prefix of a
      // protocol-stripped url such as //www.google.com/, and we don't want to
      // transform the latter into the incorrect relative url /www.google.com/.
      // If we simply require last_slash_pos >= first_slash_pos we include this
      // case, and sites like nytimes break badly.
      StringPiece base_dir(base.data() + first_slash_pos,
                           last_slash_pos-first_slash_pos+1);
      AddTrimming(base_dir);
    }
  }
}

// Left trim all strings in left_trim_strings_ from url, in order.
// StringPiece supports left and right trimming in place (the only
// mutation it permits).
bool UrlLeftTrimFilter::Trim(StringPiece* url) {
  bool trimmed = false;
  for (StringVector::iterator i = left_trim_strings_.begin();
       i != left_trim_strings_.end(); ++i) {
    // First condition below guarantees that we never completely
    // remove a url, leaving it empty.
    if (url->length() > i->length() && url->starts_with(*i)) {
      url->remove_prefix(i->length());
      trimmed = true;
    }
  }
  return trimmed;
}

// Trim the value of the given attribute, if the attribute is non-NULL.
void UrlLeftTrimFilter::TrimAttribute(HtmlElement::Attribute* attr) {
  if (attr != NULL) {
    StringPiece val(attr->value());
    size_t orig_size = val.size();
    if (Trim(&val)) {
      size_t saved = orig_size - val.size();
      const char* q = attr->quote();
      html_parse_->InfoHere(
          "trimmed %u %s=%s%s%s to %s%s%s.", static_cast<unsigned>(saved),
          attr->name().c_str(), q, attr->value(), q,
          q, val.as_string().c_str(), q);
      attr->SetValue(val);
      if (trim_count_ != NULL) {
        trim_count_->Add(1);
        trim_saved_bytes_->Add(orig_size - val.size());
      }
    }
  }
}

}  // namespace net_instaweb
