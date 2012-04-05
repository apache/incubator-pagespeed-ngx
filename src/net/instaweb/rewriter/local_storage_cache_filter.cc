/*
 * Copyright 2012 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"

#include <set>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/time_util.h"

namespace net_instaweb {

const char LocalStorageCacheFilter::kLscCookieName[] = "_GPSLSC";
const char LocalStorageCacheFilter::kLscInitializer[] =
    "pagespeed.localStorageCacheInit();";

LocalStorageCacheFilter::LocalStorageCacheFilter(RewriteDriver* rewrite_driver)
    : RewriteFilter(rewrite_driver),
      script_inserted_(false),
      script_needs_inserting_(false) {
}

LocalStorageCacheFilter::~LocalStorageCacheFilter() {
  cookie_hashes_.clear();
}

void LocalStorageCacheFilter::StartDocumentImpl() {
  script_inserted_ = false;
  script_needs_inserting_ = false;
}

void LocalStorageCacheFilter::EndDocument() {
  cookie_hashes_.clear();
}

void LocalStorageCacheFilter::StartElementImpl(HtmlElement* element) {
  // The css_inline_filter and image_rewrite_filter can add the LSC URL to
  // the inlined resource, indicating that we have to insert our JS for them.
  if (element->keyword() == HtmlName::kImg ||
      element->keyword() == HtmlName::kLink) {
    if (element->AttributeValue(HtmlName::kPagespeedLscUrl) != NULL) {
      // Note that we might end up not needing the inserted script because
      // the img/link might not be able to be inlined. So be it.
      script_needs_inserting_ = true;
    }
  }

  // We need to insert our javascript before the first element that uses it.
  if (script_needs_inserting_  && !script_inserted_) {
    InsertOurScriptElement(element);
  }
}

void LocalStorageCacheFilter::EndElementImpl(HtmlElement* element) {
  // An <img> or <link> that has a pagespeed_lsc_url attribute, and whose
  // URL's hash is in the LSC cookie, needs to be replaced by a JS snippet.
  bool is_img = false;
  bool is_link = false;
  if (element->keyword() == HtmlName::kImg) {
    is_img = true;
  } else if (element->keyword() == HtmlName::kLink) {
    is_link = true;
  }
  if (is_img || is_link) {
    const char* url = element->AttributeValue(HtmlName::kPagespeedLscUrl);
    if (url != NULL) {
      GoogleString hash = driver_->resource_manager()->hasher()->Hash(url);
      if (IsHashInCookie(driver_, kLscCookieName, hash, &cookie_hashes_)) {
        StringPiece given_url(url);
        GoogleUrl abs_url(base_url(), given_url);
        StringPiece lsc_url(abs_url.is_valid() ? abs_url.Spec() : given_url);
        GoogleString snippet;
        if (is_img) {
          snippet = StrCat("pagespeed.inlineImg(\"", lsc_url, "\"",
                           ExtractOtherImgAttributes(element), ");");
        } else /* is_link */ {
          snippet = StrCat("pagespeed.inlineCss(\"", lsc_url, "\");");
        }
        HtmlElement* script_element =
            driver_->NewElement(element->parent(), HtmlName::kScript);
        script_element->AddAttribute(
            driver_->MakeName(HtmlName::kPagespeedNoDefer), NULL, NULL);
        if (driver_->ReplaceNode(element, script_element)) {
          driver_->AppendChild(script_element,
                               driver_->NewCharactersNode(element, snippet));
        }
      }
    }
  }
}

void LocalStorageCacheFilter::InsertOurScriptElement(HtmlElement* before) {
  StaticJavascriptManager* static_js_manager =
      driver_->resource_manager()->static_javascript_manager();
  StringPiece local_storage_cache_js =
      static_js_manager->GetJsSnippet(
          StaticJavascriptManager::kLocalStorageCacheJs, driver_->options());
  const GoogleString& initialized_js = StrCat(local_storage_cache_js,
                                              kLscInitializer);
  HtmlElement* script_element = driver_->NewElement(before->parent(),
                                                    HtmlName::kScript);
  script_element->AddAttribute(driver_->MakeName(HtmlName::kPagespeedNoDefer),
                               NULL, NULL);
  driver_->InsertElementBeforeElement(before, script_element);
  HtmlNode* script_code = driver_->NewCharactersNode(script_element,
                                                     initialized_js);
  driver_->AppendChild(script_element, script_code);
  script_inserted_ = true;
}

bool LocalStorageCacheFilter::AddStorableResource(const StringPiece& url,
                                                  RewriteDriver* driver,
                                                  bool skip_cookie_check,
                                                  HtmlElement* element,
                                                  InlineState* state) {
  // Only determine the state once.
  if (!state->initialized_) {
    // If LSC isn't enabled, we're done.
    state->enabled_ =
        driver->options()->Enabled(RewriteOptions::kLocalStorageCache);

    // Get the absolute LSC url from the link url if it's valid otherwise as-is.
    if (state->enabled_) {
      GoogleUrl gurl(driver->base_url(), url);
      StringPiece best_url(gurl.is_valid() ? gurl.Spec() : url);
      best_url.CopyToString(&state->url_);
    }

    state->initialized_ = true;
  }

  if (!state->enabled_) {
    return false;
  }

  // If we've been told to skip the cookie then mark the element regardless,
  // otherwise we need to check if the hash of the url is in the LSC cookie.
  bool add_the_attr = skip_cookie_check;
  if (!skip_cookie_check) {
    RewriteFilter* filter =
        driver->FindFilter(RewriteOptions::kLocalStorageCacheId);
    if (filter != NULL) {
      LocalStorageCacheFilter* lsc =
          static_cast<LocalStorageCacheFilter*>(filter);
      GoogleString hash =
          driver->resource_manager()->hasher()->Hash(state->url_);
      add_the_attr = IsHashInCookie(driver, kLscCookieName, hash,
                                    lsc->mutable_cookie_hashes());
    }
  }

  // If necessary, set the pagespeed_lsc_url attribute in the element, which
  // later triggers the LSC filter to replace element with JS.
  if (add_the_attr) {
    driver->AddAttribute(element, HtmlName::kPagespeedLscUrl, state->url_);
  }

  return add_the_attr;
}

bool LocalStorageCacheFilter::AddLscAttributes(const StringPiece url,
                                               const CachedResult& cached,
                                               bool has_url,
                                               RewriteDriver* driver,
                                               HtmlElement* element) {
  if (!driver->options()->Enabled(RewriteOptions::kLocalStorageCache)) {
    return false;
  }

  GoogleUrl gurl(driver->base_url(), url);
  StringPiece lsc_url(gurl.is_valid() ? gurl.Spec() : url);
  GoogleString hash = driver->resource_manager()->hasher()->Hash(lsc_url);
  if (!has_url) {
    driver->AddAttribute(element, HtmlName::kPagespeedLscUrl, lsc_url);
  }
  driver->AddAttribute(element, HtmlName::kPagespeedLscHash, hash);
  if (cached.input_size() > 0) {
    const InputInfo& input_info = cached.input(0);
    if (input_info.has_expiration_time_ms()) {
      GoogleString expiry;
      if (ConvertTimeToString(input_info.expiration_time_ms(), &expiry)) {
        driver->AddAttribute(element, HtmlName::kPagespeedLscExpiry, expiry);
      }
    }
  }

  return true;
}

void LocalStorageCacheFilter::RemoveLscAttributes(HtmlElement* element) {
  element->DeleteAttribute(HtmlName::kPagespeedLscUrl);
  element->DeleteAttribute(HtmlName::kPagespeedLscHash);
  element->DeleteAttribute(HtmlName::kPagespeedLscExpiry);
  element->DeleteAttribute(HtmlName::kPagespeedNoDefer);
}

bool LocalStorageCacheFilter::IsHashInCookie(const RewriteDriver* driver,
                                             const StringPiece cookie_name,
                                             const StringPiece hash,
                                             std::set<StringPiece>* hash_set) {
  // If we have a cookie header and we haven't yet parsed it.
  if (hash_set->empty()) {
    ConstStringStarVector v;
    if (driver->request_headers()->Lookup(HttpAttributes::kCookie, &v)) {
      GoogleString prefix;
      StrAppend(&prefix, cookie_name, "=");
      for (int i = 0, nv = v.size(); i < nv; ++i) {
        StringPieceVector cookie_vector;
        SplitStringPieceToVector(*(v[i]), ";", &cookie_vector, true);
        for (int j = 0, nc = cookie_vector.size(); j < nc; ++j) {
          StringPiece cookie(cookie_vector[j]);
          TrimWhitespace(&cookie);
          if (StringCaseStartsWith(cookie, prefix)) {
            cookie.remove_prefix(prefix.length());
            StringPieceVector hashes;
            SplitStringPieceToVector(cookie, ",", &hashes, true /*omit empty*/);
            for (int k = 0, nh = hashes.size(); k < nh; ++k) {
              hash_set->insert(hashes[k]);
            }
            break;
          }
        }
      }
    }
    // If the named cookie isn't set, store a sentinel to prevent us from
    // checking again pointlessly next time we're called.
    if (hash_set->empty()) {
      hash_set->insert(StringPiece());
    }
  }
  return (hash_set->find(hash) != hash_set->end());
}

GoogleString LocalStorageCacheFilter::ExtractOtherImgAttributes(
    const HtmlElement* element) {
  // Copy over all the 'other' attributes from an img element. We do not copy:
  // pagespeed_lsc_url
  // pagespeed_lsc_hash
  // pagespeed_lsc_expiry
  // pagespeed_no_defer
  // src
  GoogleString result;
  for (int i = 0, n = element->attribute_size(); i < n; ++i) {
    const HtmlElement::Attribute& attr = element->attribute(i);
    HtmlName::Keyword keyword = attr.keyword();
    if (keyword != HtmlName::kPagespeedLscUrl &&
        keyword != HtmlName::kPagespeedLscHash &&
        keyword != HtmlName::kPagespeedLscExpiry &&
        keyword != HtmlName::kPagespeedNoDefer &&
        keyword != HtmlName::kSrc) {
      GoogleString escaped_js;
      // Escape problematic characters but don't quote it as we do that.
      if (attr.DecodedValueOrNull() != NULL) {
        EscapeToJsStringLiteral(attr.DecodedValueOrNull(), false, &escaped_js);
      }
      StrAppend(&result, ", \"", attr.name_str(), "=", escaped_js, "\"");
    }
  }
  return result;
}

}  // namespace net_instaweb
