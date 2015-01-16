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

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_hash.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"

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
  UpdateDomainHeaders(driver()->base_url(), driver()->server_context(),
                      driver()->options(),
                      driver()->mutable_response_headers());
}

DomainRewriteFilter::~DomainRewriteFilter() {}

void DomainRewriteFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kDomainRewrites);
}

void DomainRewriteFilter::UpdateDomainHeaders(
    const GoogleUrl& base_url, const ServerContext* server_context,
    const RewriteOptions* options, ResponseHeaders* headers) {
  if (headers == NULL) {
    return;
  }
  TryUpdateOneHttpDomainHeader(base_url, server_context, options,
                               HttpAttributes::kLocation, headers);
  TryUpdateOneHttpDomainHeader(base_url, server_context, options,
                               HttpAttributes::kRefresh, headers);
}

void DomainRewriteFilter::TryUpdateOneHttpDomainHeader(
    const GoogleUrl& base_url,
    const ServerContext* server_context,
    const RewriteOptions* options,
    StringPiece name,
    ResponseHeaders* headers) {
  const char* val = headers->Lookup1(name);
  if (val != NULL) {
    GoogleString new_val;
    if (UpdateOneDomainHeader(kHttp, base_url, server_context, options,
                              name, val, &new_val)) {
      headers->Replace(name, new_val);
    }
  }
}

bool DomainRewriteFilter::UpdateOneDomainHeader(
    HeaderSource src, const GoogleUrl& base_url,
    const ServerContext* server_context, const RewriteOptions* options,
    StringPiece name, StringPiece value_in, GoogleString* out) {
  bool rewrite_hyperlinks = options->domain_rewrite_hyperlinks();
  if (!rewrite_hyperlinks) {
    return false;
  }

  if (src == kHttp && StringCaseEqual(name, HttpAttributes::kLocation)) {
    DomainRewriteFilter::RewriteResult status = Rewrite(
        value_in, base_url, server_context, options,
        false /* !apply_sharding */, true /* apply_domain_suffix*/,
        out);
    return (status == kRewroteDomain);
  }

  if (StringCaseEqual(name, HttpAttributes::kRefresh)) {
    StringPiece before, url, after;
    if (ParseRefreshContent(value_in, &before, &url, &after)) {
      GoogleString rewritten_url;
      DomainRewriteFilter::RewriteResult status = Rewrite(
          url, base_url, server_context, options,
          false /* !apply_sharding */, true /* apply_domain_suffix*/,
          &rewritten_url);
      if (status == kRewroteDomain) {
        // We quote the URL with ". This is because the double-quote
        // isn't a reserved character in URLs, so %-encoding to encode any
        // pre-existing doubles quotes is safe, while doing so with single
        // quotes is not guaranteed to be a no-op.
        // (see rfc3986, 2.2)
        GlobalReplaceSubstring("\"", "%22", &rewritten_url);
        out->assign(StrCat(before, "\"", rewritten_url, "\"", after));
        return true;
      } else {
        return false;
      }
    }
  }
  // TODO(morlovich): Rewrite the domain for cookies.
  return false;
}

bool DomainRewriteFilter::ParseRefreshContent(StringPiece input,
                                              StringPiece* before,
                                              StringPiece* url,
                                              StringPiece* after) {
  // Refresh is commonly found in Http-Equiv, but also works in HTTP headers;
  // it appears to never have been spec'd for HTTP use, but thankfully
  // HTML5 specifies its syntax:
  // https://html.spec.whatwg.org/multipage/semantics.html#attr-meta-http-equiv-refresh
  // ... except that spec seems to not match reality (as tested on Chrome and
  // FF on Linux) on two points:
  // 1) Embedded whitespace is not actually stripped.
  // 2) url= is not actually required.
  StringPiece parse = input;
  TrimLeadingWhitespace(&parse);

  // Skip over the delay.
  while (!parse.empty()) {
    char inp = parse[0];
    if ((inp >= '0' && inp <= '9') || inp == '.') {
      parse.remove_prefix(1);
    } else {
      break;
    }
  }

  TrimLeadingWhitespace(&parse);
  if (parse.empty() || (parse[0] != ',' && parse[0] != ';')) {
    return false;
  }
  parse.remove_prefix(1);

  TrimLeadingWhitespace(&parse);
  // Try to match the (effectivelly optional) url=
  if (StringCaseStartsWith(parse, "url")) {
    StringPiece spec = parse;
    spec.remove_prefix(3);
    TrimLeadingWhitespace(&spec);
    if (spec.starts_with("=")) {
      spec.remove_prefix(1);
      parse = spec;
    }
  }

  // See if there is any quoting.
  TrimLeadingWhitespace(&parse);
  // ... but regardless, the pre-URL + maybe-quotes portion ends here.
  *before = StringPiece(input.data(), parse.data() - input.data());

  char quote = ' ';  // used to mark no quote.
  if (parse.starts_with("'")) {
    quote = '\'';
    parse.remove_prefix(1);
  } else if (parse.starts_with("\"")) {
    quote = '"';
    parse.remove_prefix(1);
  }

  stringpiece_ssize_type quote_pos =
      quote == ' ' ? StringPiece::npos : parse.find(quote);
  if (quote_pos != StringPiece::npos) {
    *url = parse.substr(0, quote_pos);
    const char* after_start = url->data() + url->length() + 1;
    *after = StringPiece(after_start,
                        input.data() + input.length() - after_start);
  } else {
    *url = parse;
    // Nothing after.
    *after = StringPiece();
  }
  TrimWhitespace(url);

  return !url->empty();
}

void DomainRewriteFilter::StartElementImpl(HtmlElement* element) {
  // The base URL is used to rewrite the attribute URL, which is all this
  // method does; if it isn't valid we can't so there's no point in going on.
  if (!BaseUrlIsValid()) {
    // The base URL is used to rewrite the attribute URL, which is all this
    // method does; if it isn't valid we can't so there's no point in going on.
    //
    // Note that this will be the case for any HTML elements that
    // preceed a meta tag, as the HTML spec is ambiguous whether the
    // base tag applies for that set of elements.
    return;
  }
  resource_tag_scanner::UrlCategoryVector attributes;
  const RewriteOptions* options = driver()->options();
  resource_tag_scanner::ScanElement(element, options, &attributes);
  bool element_is_embed_or_frame_or_iframe = (
      element->keyword() == HtmlName::kEmbed ||
      element->keyword() == HtmlName::kFrame ||
      element->keyword() == HtmlName::kIframe);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    // Only rewrite attributes that are resource-tags.  If hyperlinks
    // is on that's fine too.
    if (options->domain_rewrite_hyperlinks() ||
        attributes[i].category == semantic_type::kImage ||
        attributes[i].category == semantic_type::kScript ||
        attributes[i].category == semantic_type::kStylesheet) {
      StringPiece val(attributes[i].url->DecodedValueOrNull());
      if (!val.empty()) {
        GoogleString rewritten_val;
        // Don't shard hyperlinks, prefetch, embeds, frames, or iframes.
        bool apply_sharding = (
            !element_is_embed_or_frame_or_iframe &&
            attributes[i].category != semantic_type::kHyperlink &&
            attributes[i].category != semantic_type::kPrefetch);
        bool apply_domain_suffix =
              (attributes[i].category == semantic_type::kHyperlink ||
               attributes[i].category == semantic_type::kImage);
        const GoogleUrl& base_url = driver()->base_url();
        if (Rewrite(val, base_url, driver()->server_context(),
                    driver()->options(), apply_sharding, apply_domain_suffix,
                    &rewritten_val) == kRewroteDomain) {
          attributes[i].url->SetValue(rewritten_val);
          rewrite_count_->Add(1);
        }
      }
    }
  }

  // Rewrite any <meta http-equiv="a" content="b">
  if (element->keyword() == HtmlName::kMeta) {
    const char* equiv = element->AttributeValue(HtmlName::kHttpEquiv);
    HtmlElement::Attribute* content_attr =
        element->FindAttribute(HtmlName::kContent);
    const char* content = (content_attr != NULL) ?
                              content_attr->DecodedValueOrNull() : NULL;
    GoogleString out;
    if (equiv != NULL && content != NULL &&
        UpdateOneDomainHeader(kMetaHttpEquiv,
                              driver()->base_url(),
                              driver()->server_context(),
                              driver()->options(),
                              equiv,
                              content,
                              &out)) {
      content_attr->SetValue(out);
    }
  }
}

// Resolve the url we want to rewrite, and then shard as appropriate.
DomainRewriteFilter::RewriteResult DomainRewriteFilter::Rewrite(
    const StringPiece& url_to_rewrite, const GoogleUrl& base_url,
    const ServerContext* server_context, const RewriteOptions* options,
    bool apply_sharding, bool apply_domain_suffix,
    GoogleString* rewritten_url) {
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
  const DomainLawyer* lawyer = options->domain_lawyer();

  // For now, we have a proxy suffix override all other mappings.
  if (apply_domain_suffix) {
    url_to_rewrite.CopyToString(rewritten_url);
    if (lawyer->AddProxySuffix(base_url, rewritten_url)) {
      return kRewroteDomain;
    }
  }

  if (!options->IsAllowed(orig_spec) ||
      // Don't rewrite a domain from an already-rewritten resource.
      server_context->IsPagespeedResource(orig_url)) {
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
  GoogleString mapped_domain_name;
  GoogleUrl resolved_request;
  if (!lawyer->MapRequestToDomain(base_url, url_to_rewrite,
                                  &mapped_domain_name, &resolved_request,
                                  server_context->message_handler())) {
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
  if (!driver()->options()->client_domain_rewrite()) {
    return;
  }
  const DomainLawyer* lawyer = driver()->options()->domain_lawyer();
  ConstStringStarVector from_domains;
  lawyer->FindDomainsRewrittenTo(driver()->base_url(), &from_domains);

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

  HtmlElement* script_node = driver()->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script_node);
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  GoogleString js =
      StrCat(static_asset_manager->GetAsset(
                 StaticAssetEnum::CLIENT_DOMAIN_REWRITER,
                 driver()->options()),
             "pagespeed.clientDomainRewriterInit([",
             comma_separated_from_domains, "]);");
  AddJsToElement(js, script_node);
}

}  // namespace net_instaweb
