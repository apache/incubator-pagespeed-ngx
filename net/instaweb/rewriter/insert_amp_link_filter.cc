/*
 * Copyright 2016 Google Inc.
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

// Author: sjnickerson@google.com (Simon Nickerson)

#include "net/instaweb/rewriter/public/insert_amp_link_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

namespace {

// Attribute name for the AMP <link> tag
const char kAmpHtmlAttributeName[] = "amphtml";

// URL fragment names for matching. These are case sensitive.
const char kAllExceptLeaf[] = "all_except_leaf";
const char kAllExceptQuery[] = "all_except_query";
const char kLeafSansQuery[] = "leaf_sans_query";
const char kLeafWithQuery[] = "leaf_with_query";
const char kPathNoTrailingSlash[] = "path_no_trailing_slash";
const char kMaybeTrailingSlash[] = "maybe_trailing_slash";
const char kMaybeQuery[] = "maybe_query";
const char kUrlPattern[] = "url";
const char kTrailingSlash[] = "/";

GoogleString PathNoTrailingSlash(const GoogleUrl& google_url);
GoogleString MaybeQuery(const GoogleUrl& google_url);
GoogleString MaybeTrailingSlash(const GoogleUrl& google_url);
void AppendUrlFragment(GoogleString* str, const GoogleUrl& google_url,
                          const StringPiece fragment_name);

}  // namespace

InsertAmpLinkFilter::InsertAmpLinkFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver), enabled_(false), amp_link_found_(false) {}

InsertAmpLinkFilter::~InsertAmpLinkFilter() {}

void InsertAmpLinkFilter::StartDocumentImpl() { amp_link_found_ = false; }

void InsertAmpLinkFilter::DetermineEnabled(GoogleString* disabled_reason) {
  const RewriteOptions* options = driver()->options();
  if (options->amp_link_pattern() != "") {
    set_is_enabled(true);
  }
}

void InsertAmpLinkFilter::StartElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kLink &&
      StringCaseEqual(element->AttributeValue(HtmlName::kRel),
                      kAmpHtmlAttributeName)) {
    amp_link_found_ = true;
  }
}

void InsertAmpLinkFilter::EndElementImpl(HtmlElement* element) {
  if (is_enabled() && !amp_link_found_ &&
      element->keyword() == HtmlName::kHead) {
    HtmlElement* linkAmphtml = driver()->NewElement(element, HtmlName::kLink);
    driver()->AddAttribute(linkAmphtml, HtmlName::kRel, kAmpHtmlAttributeName);
    driver()->AddAttribute(linkAmphtml, HtmlName::kHref, GetAmpUrl());
    driver()->AppendChild(element, linkAmphtml);
    // We don't want to insert AMP links if there are multiple <head> elements.
    amp_link_found_ = true;
  }
}

GoogleString InsertAmpLinkFilter::GetAmpUrl() {
  const char* amp_pattern = driver()->options()->amp_link_pattern().c_str();
  const GoogleUrl& google_url = driver()->google_url();

  GoogleString amp_link;

  // Populate amp_link by iterating through amp_pattern. Characters between "${"
  // and "}" are URL fragment names. Other characters are just copied directly.
  const char* fragment_name_start = nullptr;
  for (const char* p = amp_pattern; *p != '\0'; p++) {
    if (*p == '$' && *(p + 1) == '{') {
      fragment_name_start = p + 2;
    } else if (*p == '}' && fragment_name_start != nullptr) {
      StringPiece fragment_name(fragment_name_start, p - fragment_name_start);
      AppendUrlFragment(&amp_link, google_url, fragment_name);
      fragment_name_start = nullptr;
    } else if (fragment_name_start == nullptr) {
      amp_link += *p;
    }
  }

  if (fragment_name_start != nullptr) {
    // Handle missing closing brace by appending the string literal.
    // The "- 2" is to pick up the "${".
    StrAppend(&amp_link, fragment_name_start - 2);
  }

  return amp_link;
}

namespace {

// Appends a fragment_name extracted from google_url to str.
void AppendUrlFragment(GoogleString* str, const GoogleUrl& google_url,
                          const StringPiece fragment_name) {
  if (fragment_name == kUrlPattern) {
    StrAppend(str, google_url.Spec());
  } else if (fragment_name == kAllExceptQuery) {
    StrAppend(str, google_url.AllExceptQuery());
  } else if (fragment_name == kAllExceptLeaf) {
    StrAppend(str, google_url.AllExceptLeaf());
  } else if (fragment_name == kLeafSansQuery) {
    StrAppend(str, google_url.LeafSansQuery());
  } else if (fragment_name == kLeafWithQuery) {
    StrAppend(str, google_url.LeafWithQuery());
  } else if (fragment_name == kMaybeQuery) {
    StrAppend(str, MaybeQuery(google_url));
  } else if (fragment_name == kPathNoTrailingSlash) {
    StrAppend(str, PathNoTrailingSlash(google_url));
  } else if (fragment_name == kMaybeTrailingSlash) {
    StrAppend(str, MaybeTrailingSlash(google_url));
  } else {
    // If we couldn't find a match, just append fragment_name between "${" and
    // "}" as it would have appeared in the amp_link_pattern.
    StrAppend(str, "${", fragment_name, "}");
  }
}

// Returns the path from google_url without the trailing slash (if present).
// For example, "http://test.com/a/b/" -> "/a/b"
GoogleString PathNoTrailingSlash(const GoogleUrl& google_url) {
  GoogleString path =
      StrCat(google_url.PathSansLeaf(), google_url.LeafSansQuery());
  if (EndsInSlash(path)) {
    path.resize(path.size() - 1);
  }
  return path;
}

// Returns a trailing slash if the path in the google_url contains a trailing
// slash. For example, "http://test.com/a/b/" -> "/"
GoogleString MaybeQuery(const GoogleUrl& google_url) {
  StringPiece query = google_url.Query();
  GoogleString maybe_query = query.empty() ? "" : StrCat("?", query);
  return maybe_query;
}

// Returns the query string including a "?" if present, or an empty string
// if not present. For example, "http://test.com/a/b?p=3" -> "?p=3"
GoogleString MaybeTrailingSlash(const GoogleUrl& google_url) {
  if (google_url.AllExceptQuery().ends_with(kTrailingSlash)) {
    return kTrailingSlash;
  } else {
    return "";
  }
}

}  // namespace

}  // namespace net_instaweb
