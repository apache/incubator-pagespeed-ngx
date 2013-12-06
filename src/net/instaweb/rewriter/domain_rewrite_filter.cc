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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"

#include <memory>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// Names for Statistics variables.
const char kDomainRewrites[] = "domain_rewrites";

}  // namespace

namespace net_instaweb {

DomainRewriteFilter::DomainRewriteFilter(RewriteDriver* rewrite_driver,
                                         Statistics *stats)
    : CommonFilter(rewrite_driver),
      rewrite_count_(stats->GetVariable(kDomainRewrites)) {}

void DomainRewriteFilter::StartDocumentImpl() {
  bool rewrite_hyperlinks = driver_->options()->domain_rewrite_hyperlinks();

  if (rewrite_hyperlinks) {
    // TODO(nikhilmadan): Rewrite the domain for cookies.
    // Rewrite the Location header for redirects.
    UpdateLocationHeader(driver_->base_url(), driver_,
                         driver_->mutable_response_headers());
  }
}

DomainRewriteFilter::~DomainRewriteFilter() {}

void DomainRewriteFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kDomainRewrites);
}

void DomainRewriteFilter::UpdateLocationHeader(const GoogleUrl& base_url,
                                               RewriteDriver* driver,
                                               ResponseHeaders* headers) const {
  if (headers != NULL) {
    const char* location = headers->Lookup1(HttpAttributes::kLocation);
    if (location != NULL) {
      GoogleString new_location;
      DomainRewriteFilter::RewriteResult status = Rewrite(
          location, base_url, driver, false /* !apply_sharding */,
          &new_location);
      if (status == kRewroteDomain) {
        headers->Replace(HttpAttributes::kLocation, new_location);
      }
    }
  }
}

void DomainRewriteFilter::StartElementImpl(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    // Disable domain_rewrite for non-image, non-script, non-stylesheet urls
    // unless ModPagespeedDomainRewriteHyperlinks is on
    if (attributes[i].category != semantic_type::kImage &&
        attributes[i].category != semantic_type::kScript &&
        attributes[i].category != semantic_type::kStylesheet &&
        !driver_->options()->domain_rewrite_hyperlinks()) {
      continue;
    }
    StringPiece val(attributes[i].url->DecodedValueOrNull());
    GoogleString rewritten_val;
    // Don't shard hyperlinks, prefetch, embeds, frames, or iframes.
    bool apply_sharding = (
        attributes[i].category != semantic_type::kHyperlink &&
        attributes[i].category != semantic_type::kPrefetch &&
        element->keyword() != HtmlName::kEmbed &&
        element->keyword() != HtmlName::kFrame &&
        element->keyword() != HtmlName::kIframe);
    if (!val.empty() && BaseUrlIsValid() &&
        (Rewrite(val, driver_->base_url(), driver_,
                 apply_sharding, &rewritten_val) == kRewroteDomain)) {
      attributes[i].url->SetValue(rewritten_val);
      rewrite_count_->Add(1);
    }
  }
}

// Resolve the url we want to rewrite, and then shard as appropriate.
DomainRewriteFilter::RewriteResult DomainRewriteFilter::Rewrite(
    const StringPiece& url_to_rewrite, const GoogleUrl& base_url,
    const RewriteDriver* driver,
    bool apply_sharding, GoogleString* rewritten_url) const {
  if (url_to_rewrite.empty()) {
    rewritten_url->clear();
    return kDomainUnchanged;
  }

  GoogleUrl orig_url(base_url, url_to_rewrite);
  if (!orig_url.IsWebOrDataValid()) {
    return kFail;
  }

  if (!orig_url.IsWebValid()) {
    url_to_rewrite.CopyToString(rewritten_url);
    return kDomainUnchanged;
  }

  StringPiece orig_spec = orig_url.Spec();
  const RewriteOptions* options = driver->options();

  if (!options->IsAllowed(orig_spec) ||
      // Don't rewrite a domain from an already-rewritten resource.
      server_context_->IsPagespeedResource(orig_url)) {
    // Even though domain is unchanged, we need to store absolute URL in
    // rewritten_url.
    orig_url.Spec().CopyToString(rewritten_url);
    return kDomainUnchanged;
  }

  // Apply any domain rewrites.
  //
  // TODO(jmarantz): There are two things going on: resolving URLs
  // against base and mapping them.  We should (a) factor those out
  // so they are distinct and (b) only do the resolution once, as it
  // is expensive.  I think the ResourceSlot system offers a good
  // framework to do this.
  const DomainLawyer* lawyer = options->domain_lawyer();
  GoogleString mapped_domain_name;
  GoogleUrl resolved_request;
  if (!lawyer->MapRequestToDomain(base_url, url_to_rewrite,
                                  &mapped_domain_name, &resolved_request,
                                  driver->message_handler())) {
    // Even though domain is unchanged, we need to store absolute URL in
    // rewritten_url.
    orig_url.Spec().CopyToString(rewritten_url);
    return kDomainUnchanged;
  }

  // Next, apply any sharding.
  GoogleString sharded_domain;
  GoogleString domain = StrCat(resolved_request.Origin(), "/");
  resolved_request.Spec().CopyToString(rewritten_url);
  uint32 int_hash = HashString<CasePreserve, uint32>(
      rewritten_url->data(), rewritten_url->size());
  if (apply_sharding &&
      lawyer->ShardDomain(domain, int_hash, &sharded_domain)) {
    *rewritten_url = StrCat(sharded_domain,
                            resolved_request.PathAndLeaf().substr(1));
  }

  // Return true if really changed the url with this rewrite.
  if (orig_spec == *rewritten_url) {
    return kDomainUnchanged;
  } else {
    return kRewroteDomain;
  }
}

void DomainRewriteFilter::EndDocument() {
  if (!driver_->options()->client_domain_rewrite()) {
    return;
  }
  const DomainLawyer* lawyer = driver_->options()->domain_lawyer();
  ConstStringStarVector from_domains;
  lawyer->FindDomainsRewrittenTo(driver_->base_url(), &from_domains);

  if (from_domains.empty()) {
    return;
  }

  GoogleString comma_separated_from_domains;
  for (int i = 0, n = from_domains.size(); i < n; i++) {
    StrAppend(&comma_separated_from_domains, "\"", *(from_domains[i]), "\"");
    if (i != n - 1) {
      StrAppend(&comma_separated_from_domains, ",");
    }
  }

  HtmlElement* script_node = driver_->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script_node);
  StaticAssetManager* static_asset_manager =
      driver_->server_context()->static_asset_manager();
  GoogleString js =
      StrCat(static_asset_manager->GetAsset(
                 StaticAssetManager::kClientDomainRewriter, driver_->options()),
             "pagespeed.clientDomainRewriterInit([",
             comma_separated_from_domains, "]);");
  static_asset_manager->AddJsToElement(js, script_node, driver_);
}

}  // namespace net_instaweb
