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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
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
class RewriteFilter;

DomainRewriteFilter::DomainRewriteFilter(RewriteDriver* rewrite_driver,
                                     Statistics *stats)
    : CommonFilter(rewrite_driver),
      tag_scanner_(rewrite_driver),
      rewrite_count_(stats->GetVariable(kDomainRewrites)) {
  tag_scanner_.set_find_a_tags(false);
}

void DomainRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kDomainRewrites);
}

void DomainRewriteFilter::StartElementImpl(HtmlElement* element) {
  // Disable domain_rewrite for img is ModPagespeedDisableForBots is on
  // and the user-agent is a bot.
  if (element->keyword() == HtmlName::kImg &&
      driver_->RewriteImages()) {
    return;
  }
  HtmlElement::Attribute* attr = tag_scanner_.ScanElement(element);
  if (attr != NULL) {
    StringPiece val(attr->value());
    GoogleString rewritten_val;
    if (Rewrite(val, driver_->base_url(), &rewritten_val)) {
      attr->SetValue(rewritten_val);
      rewrite_count_->Add(1);
    }
  }
}

// Resolve the url we want to rewrite, and then shard as appropriate.
bool DomainRewriteFilter::Rewrite(const StringPiece& url_to_rewrite,
                                  const GoogleUrl& base_url,
                                  GoogleString* rewritten_url) {
  if (url_to_rewrite.empty() || !BaseUrlIsValid()) {
    return false;
  }

  GoogleUrl orig_url(base_url, url_to_rewrite);
  StringPiece orig_spec = orig_url.Spec();
  const RewriteOptions* options = driver_->options();
  if (!orig_url.is_valid() || !orig_url.is_standard() ||
      !options->IsAllowed(orig_spec)) {
    return false;
  }

  // Don't rewrite a domain from an already-rewritten resource.
  {
    RewriteFilter* filter;
    OutputResourcePtr output_resource(driver_->DecodeOutputResource(
        orig_spec, &filter));
    if (output_resource.get() != NULL) {
      return false;
    }
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
                                  driver_->message_handler())) {
    return false;
  }

  // Next, apply any sharding.
  GoogleString shard, domain = StrCat(resolved_request.Origin(), "/");
  resolved_request.Spec().CopyToString(rewritten_url);
  uint32 int_hash = HashString<CasePreserve, uint32>(
      rewritten_url->data(), rewritten_url->size());
  if (lawyer->ShardDomain(domain, int_hash, &shard)) {
    *rewritten_url = StrCat(shard, resolved_request.Path().substr(1));
  }

  // Return true if really changed the url with this rewrite.
  return orig_spec != *rewritten_url;
}

}  // namespace net_instaweb
