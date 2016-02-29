/*
 * Copyright 2014 Google Inc.
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

// Author: stevensr@google.com (Ryan Stevens)

#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"

#include "net/instaweb/rewriter/mobilize_cached.pb.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/mobilize_cached_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

const char MobilizeRewriteFilter::kPagesMobilized[] =
    "mobilization_pages_rewritten";

namespace {

GoogleString FormatColorForJs(const RewriteOptions::Color& color) {
  return StrCat("[",
                IntegerToString(color.r), ",",
                IntegerToString(color.g), ",",
                IntegerToString(color.b), "]");
}

void ConvertColor(const MobilizeCached::Color& color,
                  RewriteOptions::Color* out) {
  out->r = color.r();
  out->g = color.g();
  out->b = color.b();
}

}  // namespace

MobilizeRewriteFilter::MobilizeRewriteFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      body_element_depth_(0),
      added_viewport_(false),
      added_style_(false),
      added_spacer_(false),
      saw_end_document_(false) {
  // If a domain proxy-suffix is specified, and it starts with ".",
  // then we'll remove the "." from that and use that as the location
  // of the shared static files (JS and CSS).  E.g.
  // for a proxy_suffix of ".suffix" we'll look for static files in
  // "//suffix/static/".
  StringPiece suffix(
      rewrite_driver->options()->domain_lawyer()->proxy_suffix());
  if (!suffix.empty() && suffix.starts_with(".")) {
    suffix.remove_prefix(1);
    static_file_prefix_ = StrCat("//", suffix, "/static/");
  }
  Statistics* stats = rewrite_driver->statistics();
  num_pages_mobilized_ = stats->GetVariable(kPagesMobilized);
}

MobilizeRewriteFilter::~MobilizeRewriteFilter() {}

void MobilizeRewriteFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kPagesMobilized);
}

bool MobilizeRewriteFilter::IsApplicableFor(RewriteDriver* driver) {
  return IsApplicableFor(driver->options(), driver->user_agent().c_str(),
                         driver->server_context()->user_agent_matcher());
}

bool MobilizeRewriteFilter::IsApplicableFor(const RewriteOptions* options,
                                            const char* user_agent,
                                            const UserAgentMatcher* matcher) {
  // Note: we may need to narrow the set of applicable user agents here, but for
  // now we (very) optimistically assume that our JS works on any mobile UA.
  // TODO(jmaessen): Some debate over whether to include tablet UAs here.  We
  // almost certainly want touch-friendliness, but the geometric constraints are
  // very different and we probably want to turn off almost all non-navigational
  // mobilization.
  // TODO(jmaessen): If we want to inject instrumentation on desktop pages to
  // beacon back data useful for mobile page views, this should change and we'll
  // want to check at code injection points instead.
  return options->mob_always() ||
      (matcher->GetDeviceTypeForUA(user_agent) == UserAgentMatcher::kMobile);
}

void MobilizeRewriteFilter::DetermineEnabled(GoogleString* disabled_reason) {
  if (!IsApplicableFor(driver())) {
    disabled_reason->assign("Not a mobile User Agent.");
    set_is_enabled(false);
  }
}

void MobilizeRewriteFilter::StartDocumentImpl() { saw_end_document_ = false; }

void MobilizeRewriteFilter::EndDocument() {
  saw_end_document_ = true;
  num_pages_mobilized_->Add(1);
  body_element_depth_ = 0;
  added_viewport_ = false;
  added_style_ = false;
  added_spacer_ = false;
}

GoogleString MobilizeRewriteFilter::GetMobJsInitScript() {
  // Transmit to the mobilization scripts whether they are run in debug
  // mode or not by setting 'psDebugMode'.
  //
  // Also, transmit to the mobilization scripts whether navigation is
  // enabled.  That is bundled into the same JS compile unit as the
  // layout, so we cannot do a 'undefined' check in JS to determine
  // whether it was enabled.
  GoogleString src =
      StrCat("window.psDebugMode=", BoolToString(driver()->DebugMode()),
             ";window.psDeviceType='",
             UserAgentMatcher::DeviceTypeString(
                 driver()->request_properties()->GetDeviceType()),
             "';");
  const RewriteOptions* options = driver()->options();
  const GoogleString& phone = options->mob_phone_number();
  const GoogleString& map_location = options->mob_map_location();
  if (!phone.empty() || !map_location.empty()) {
    StrAppend(&src, "window.psConversionId='",
              Integer64ToString(options->mob_conversion_id()), "';");
  }
  if (!phone.empty()) {
    GoogleString label, escaped_phone;
    EscapeToJsStringLiteral(phone, false, &escaped_phone);
    EscapeToJsStringLiteral(options->mob_phone_conversion_label(), false,
                            &label);
    StrAppend(&src, "window.psPhoneNumber='", escaped_phone,
              "';window.psPhoneConversionLabel='", label, "';");
  }
  if (!map_location.empty()) {
    GoogleString label, escaped_map_location;
    EscapeToJsStringLiteral(map_location, false, &escaped_map_location);
    EscapeToJsStringLiteral(options->mob_map_conversion_label(), false, &label);
    StrAppend(&src, "window.psMapLocation='", escaped_map_location,
              "';window.psMapConversionLabel='", label, "';");
  }

  // See if we have a precomputed theme, either via options or pcache.
  bool has_mob_theme = false;
  RewriteOptions::Color background_color, foreground_color;
  GoogleString logo_url;
  if (options->has_mob_theme()) {
    has_mob_theme = true;
    background_color = options->mob_theme().background_color;
    foreground_color = options->mob_theme().foreground_color;
    logo_url = options->mob_theme().logo_url;
  } else {
    MobilizeCachedFinder* finder =
        driver()->server_context()->mobilize_cached_finder();
    MobilizeCached out;
    if (finder && finder->GetMobilizeCachedFromPropertyCache(driver(), &out)) {
      has_mob_theme = out.has_background_color() && out.has_foreground_color();
      ConvertColor(out.background_color(), &background_color);
      ConvertColor(out.foreground_color(), &foreground_color);
      logo_url = out.foreground_image_url();
    }
  }

  if (has_mob_theme) {
    StrAppend(&src, "window.psMobBackgroundColor=",
              FormatColorForJs(background_color), ";");
    StrAppend(&src, "window.psMobForegroundColor=",
              FormatColorForJs(foreground_color), ";");
  } else {
    StrAppend(&src, "window.psMobBackgroundColor=null;");
    StrAppend(&src, "window.psMobForegroundColor=null;");
  }
  GoogleString escaped_mob_beacon_url;
  EscapeToJsStringLiteral(options->mob_beacon_url(), false /* add_quotes */,
                          &escaped_mob_beacon_url);
  StrAppend(&src, "window.psMobBeaconUrl='", escaped_mob_beacon_url, "';");

  if (!options->mob_beacon_category().empty()) {
    GoogleString escaped_mob_beacon_cat;
    EscapeToJsStringLiteral(options->mob_beacon_category(),
                            false, /* add_quotes */
                            &escaped_mob_beacon_cat);
    StrAppend(&src, "window.psMobBeaconCategory='", escaped_mob_beacon_cat,
              "';");
  }
  return src;
}

void MobilizeRewriteFilter::RenderDone() {
  // We insert the JS using RenderDone() because it needs to be inserted after
  // MobilizeMenuRenderFilter finishes inserting the nav panel element, and this
  // is how the nav panel is inserted.
  if (!saw_end_document_) {
    return;
  }

  StaticAssetManager* manager =
      driver()->server_context()->static_asset_manager();
  StringPiece js =
      manager->GetAssetUrl(StaticAssetEnum::MOBILIZE_JS, driver()->options());
  HtmlElement* script_element =
      driver()->NewElement(nullptr, HtmlName::kScript);
  InsertNodeAtBodyEnd(script_element);
  driver()->AddAttribute(script_element, HtmlName::kSrc, js);

  // Insert a script tag with the global config variable assignments, and the
  // call to psStartMobilization.
  HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script);
  HtmlNode* text_node = driver()->NewCharactersNode(
      script, StrCat(GetMobJsInitScript(), "psStartMobilization();"));
  driver()->AppendChild(script, text_node);
}

void MobilizeRewriteFilter::StartElementImpl(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();

  if (keyword == HtmlName::kHead) {
    // <meta name="viewport"... />
    if (!added_viewport_) {
      added_viewport_ = true;
      const RewriteOptions* options = driver()->options();
      const GoogleString& phone = options->mob_phone_number();
      if (!phone.empty()) {
        // Insert <meta itemprop="telephone" content="+18005551212">
        HtmlElement* telephone_meta_element = driver()->NewElement(
            element, HtmlName::kMeta);
        telephone_meta_element->set_style(HtmlElement::BRIEF_CLOSE);
        telephone_meta_element->AddAttribute(
            driver()->MakeName(HtmlName::kItemProp), "telephone",
            HtmlElement::DOUBLE_QUOTE);
        telephone_meta_element->AddAttribute(
            driver()->MakeName(HtmlName::kContent), phone,
            HtmlElement::DOUBLE_QUOTE);
        driver()->InsertNodeAfterCurrent(telephone_meta_element);
      }
    }
  } else if (keyword == HtmlName::kBody) {
    ++body_element_depth_;
    if (!added_spacer_) {
      added_spacer_ = true;

      // TODO(jmaessen): Right now we inject an unstyled, unsized header bar.
      // This actually works OK in testing on current sites, because nav.js
      // styles and sizes it at onload.  We should style it using mob_theme_data
      // when that's available.
      HtmlElement* header = driver()->NewElement(element, HtmlName::kHeader);
      driver()->InsertNodeAfterCurrent(header);
      driver()->AddAttribute(header, HtmlName::kId, "psmob-header-bar");
      // Make sure that the header bar is not displayed until the redraw
      // function is called to set font-size. Otherwise the header bar will be
      // too large, causing the iframe to be too small.
      driver()->AddAttribute(header, HtmlName::kClass, "psmob-hide");

      // The spacer is added by IframeFetcher when iframe mode is enabled.
      if (!driver()->options()->mob_iframe()) {
        HtmlElement* spacer = driver()->NewElement(element, HtmlName::kDiv);
        driver()->InsertNodeAfterCurrent(spacer);
        driver()->AddAttribute(spacer, HtmlName::kId, "psmob-spacer");
      }
    }
  }
}

void MobilizeRewriteFilter::EndElementImpl(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();

  if (keyword == HtmlName::kBody) {
    --body_element_depth_;
  } else if (body_element_depth_ == 0 && keyword == HtmlName::kHead) {
    // TODO(jmarantz): this uses AppendChild, but probably should use
    // InsertBeforeCurrent to make it work with flush windows.
    AddStyle(element);
  }
}

void MobilizeRewriteFilter::AppendStylesheet(StringPiece css_file_name,
                                             StaticAssetEnum::StaticAsset asset,
                                             HtmlElement* element) {
  HtmlElement* link = driver()->NewElement(element, HtmlName::kLink);
  driver()->AppendChild(element, link);
  driver()->AddAttribute(link, HtmlName::kRel, "stylesheet");
  StaticAssetManager* manager =
      driver()->server_context()->static_asset_manager();
  StringPiece css = manager->GetAssetUrl(asset, driver()->options());
  driver()->AddAttribute(link, HtmlName::kHref, css);
}

void MobilizeRewriteFilter::AddStyle(HtmlElement* element) {
  if (!added_style_) {
    added_style_ = true;
    AppendStylesheet("mobilize.css", StaticAssetEnum::MOBILIZE_CSS, element);
  }
}

}  // namespace net_instaweb
