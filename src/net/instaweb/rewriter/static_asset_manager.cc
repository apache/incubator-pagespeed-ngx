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

// Author: guptaa@google.com (Ashish Gupta)

#include "net/instaweb/rewriter/public/static_asset_manager.h"

#include <cstddef>
#include <memory>
#include <utility>
#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/doctype.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

extern const char* JS_add_instrumentation;
extern const char* JS_add_instrumentation_opt;
extern const char* JS_client_domain_rewriter;
extern const char* JS_client_domain_rewriter_opt;
extern const char* JS_critical_css_beacon;
extern const char* JS_critical_css_beacon_opt;
extern const char* JS_critical_css_loader;
extern const char* JS_critical_css_loader_opt;
extern const char* JS_critical_images_beacon;
extern const char* JS_critical_images_beacon_opt;
extern const char* JS_dedup_inlined_images;
extern const char* JS_dedup_inlined_images_opt;
extern const char* JS_defer_iframe;
extern const char* JS_defer_iframe_opt;
extern const char* JS_delay_images;
extern const char* JS_delay_images_inline;
extern const char* JS_delay_images_inline_opt;
extern const char* JS_delay_images_opt;
extern const char* JS_deterministic;
extern const char* JS_deterministic_opt;
extern const char* JS_extended_instrumentation;
extern const char* JS_extended_instrumentation_opt;
extern const char* JS_ghost_click_buster_opt;
extern const char* JS_js_defer;
extern const char* JS_js_defer_opt;
extern const char* JS_lazyload_images;
extern const char* JS_lazyload_images_opt;
extern const char* JS_local_storage_cache;
extern const char* JS_local_storage_cache_opt;
extern const char* JS_panel_loader_opt;
extern const char* JS_split_html_beacon;
extern const char* JS_split_html_beacon_opt;

// TODO(jud): use the data2c build flow to create this data.
const unsigned char GIF_blank[] = {
  0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x1, 0x0, 0x1,
  0x0, 0x80, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x21, 0xfe, 0x6,
  0x70, 0x73, 0x61, 0x5f, 0x6c, 0x6c, 0x0, 0x21, 0xf9, 0x4, 0x1, 0xa, 0x0, 0x1,
  0x0, 0x2c, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x2, 0x2, 0x4c, 0x1,
  0x0, 0x3b };
const int GIF_blank_len = arraysize(GIF_blank);

// The generated files(blink.js, js_defer.js) are named in "<hash>-<fileName>"
// format.
const char StaticAssetManager::kGStaticBase[] =
    "//www.gstatic.com/psa/static/";

// TODO(jud): Change to "/psaassets/".
const char StaticAssetManager::kDefaultLibraryUrlPrefix[] = "/psajs/";

// TODO(jud): Refactor this struct so that each static type served (js, images,
// etc.) has it's own implementation.
struct StaticAssetManager::Asset {
  const char* file_name;
  GoogleString js_optimized;
  GoogleString js_debug;
  GoogleString js_opt_hash;
  GoogleString js_debug_hash;
  GoogleString opt_url;
  GoogleString debug_url;
  ContentType content_type;
};

StaticAssetManager::StaticAssetManager(
    const GoogleString& static_asset_base,
    Hasher* hasher,
    MessageHandler* message_handler)
    : static_asset_base_(static_asset_base),
      hasher_(hasher),
      message_handler_(message_handler),
      serve_asset_from_gstatic_(false),
      library_url_prefix_(kDefaultLibraryUrlPrefix) {
  InitializeAssetStrings();

  // Note: We use these default options because the actual options will
  // not affect what we are computing here.
  ResponseHeaders header(kDeprecatedDefaultHttpOptions);
  header.SetDateAndCaching(0, ServerContext::kCacheTtlForMismatchedContentMs);
  cache_header_with_private_ttl_ = StrCat(
      header.Lookup1(HttpAttributes::kCacheControl),
      ",private");

  header.Clear();
  header.SetDateAndCaching(0, ServerContext::kGeneratedMaxAgeMs);
  cache_header_with_long_ttl_ = header.Lookup1(HttpAttributes::kCacheControl);
}

StaticAssetManager::~StaticAssetManager() {
  STLDeleteElements(&assets_);
}

const GoogleString& StaticAssetManager::GetAssetUrl(
    StaticAssetEnum::StaticAsset module, const RewriteOptions* options) const {
  return options->Enabled(RewriteOptions::kDebug) ?
      assets_[module]->debug_url :
      assets_[module]->opt_url;
}

void StaticAssetManager::set_gstatic_hash(StaticAssetEnum::StaticAsset module,
                                          const GoogleString& gstatic_base,
                                          const GoogleString& hash) {
  if (serve_asset_from_gstatic_) {
    CHECK(!hash.empty());
    assets_[module]->opt_url =
        StrCat(gstatic_base, hash, "-", assets_[module]->file_name,
               assets_[module]->content_type.file_extension());
  }
}

void StaticAssetManager::InitializeAssetStrings() {
  assets_.resize(StaticAssetEnum::StaticAsset_ARRAYSIZE);
  for (std::vector<Asset*>::iterator it = assets_.begin();
       it != assets_.end(); ++it) {
    *it = new Asset;
    (*it)->content_type = kContentTypeJavascript;
  }
  // Initialize JS
  // Initialize file names.
  assets_[StaticAssetEnum::ADD_INSTRUMENTATION_JS]->file_name =
      "add_instrumentation";
  assets_[StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS]->file_name =
      "extended_instrumentation";
  GoogleString blink_js_string =
      StrCat(JS_js_defer_opt, "\n", JS_panel_loader_opt);
  assets_[StaticAssetEnum::BLINK_JS]->file_name = "blink";
  assets_[StaticAssetEnum::CLIENT_DOMAIN_REWRITER]->file_name =
      "client_domain_rewriter";
  assets_[StaticAssetEnum::CRITICAL_CSS_BEACON_JS]->file_name =
      "critical_css_beacon";
  assets_[StaticAssetEnum::CRITICAL_CSS_LOADER_JS]->file_name =
      "critical_css_loader";
  assets_[StaticAssetEnum::CRITICAL_IMAGES_BEACON_JS]->file_name =
      "critical_images_beacon";
  assets_[StaticAssetEnum::DEDUP_INLINED_IMAGES_JS]->file_name =
      "dedup_inlined_images";
  assets_[StaticAssetEnum::DEFER_IFRAME]->file_name = "defer_iframe";
  assets_[StaticAssetEnum::DEFER_JS]->file_name = "js_defer";
  assets_[StaticAssetEnum::DELAY_IMAGES_JS]->file_name = "delay_images";
  assets_[StaticAssetEnum::DELAY_IMAGES_INLINE_JS]->file_name =
      "delay_images_inline";
  assets_[StaticAssetEnum::LAZYLOAD_IMAGES_JS]->file_name = "lazyload_images";
  assets_[StaticAssetEnum::DETERMINISTIC_JS]->file_name = "deterministic";
  assets_[StaticAssetEnum::GHOST_CLICK_BUSTER_JS]->file_name =
      "ghost_click_buster";
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->file_name =
      "local_storage_cache";
  assets_[StaticAssetEnum::SPLIT_HTML_BEACON_JS]->file_name =
      "split_html_beacon";

  // Initialize compiled javascript strings->
  assets_[StaticAssetEnum::ADD_INSTRUMENTATION_JS]->js_optimized =
      JS_add_instrumentation_opt;
  assets_[StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS]->js_optimized =
      JS_extended_instrumentation_opt;
  assets_[StaticAssetEnum::BLINK_JS]->js_optimized = blink_js_string;
  assets_[StaticAssetEnum::CLIENT_DOMAIN_REWRITER]->js_optimized =
      JS_client_domain_rewriter_opt;
  assets_[StaticAssetEnum::CRITICAL_CSS_BEACON_JS]->js_optimized =
      JS_critical_css_beacon_opt;
  assets_[StaticAssetEnum::CRITICAL_CSS_LOADER_JS]->js_optimized =
      JS_critical_css_loader_opt;
  assets_[StaticAssetEnum::CRITICAL_IMAGES_BEACON_JS]->js_optimized =
      JS_critical_images_beacon_opt;
  assets_[StaticAssetEnum::DEDUP_INLINED_IMAGES_JS]->js_optimized =
      JS_dedup_inlined_images_opt;
  assets_[StaticAssetEnum::DEFER_IFRAME]->js_optimized =
      JS_defer_iframe_opt;
  assets_[StaticAssetEnum::DEFER_JS]->js_optimized =
      JS_js_defer_opt;
  assets_[StaticAssetEnum::DELAY_IMAGES_JS]->js_optimized =
      JS_delay_images_opt;
  assets_[StaticAssetEnum::DELAY_IMAGES_INLINE_JS]->js_optimized =
      JS_delay_images_inline_opt;
  assets_[StaticAssetEnum::LAZYLOAD_IMAGES_JS]->js_optimized =
      JS_lazyload_images_opt;
  assets_[StaticAssetEnum::DETERMINISTIC_JS]->js_optimized =
      JS_deterministic_opt;
  assets_[StaticAssetEnum::GHOST_CLICK_BUSTER_JS]->js_optimized =
      JS_ghost_click_buster_opt;
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->js_optimized =
      JS_local_storage_cache_opt;
  assets_[StaticAssetEnum::SPLIT_HTML_BEACON_JS]->js_optimized =
      JS_split_html_beacon_opt;

  // Initialize cleartext javascript strings->
  assets_[StaticAssetEnum::ADD_INSTRUMENTATION_JS]->js_debug =
      JS_add_instrumentation;
  assets_[StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS]->js_debug =
      JS_extended_instrumentation;
  // Fetching the blink JS is not currently supported-> Add a comment in as the
  // unit test expects debug code to include comments->
  assets_[StaticAssetEnum::BLINK_JS]->js_debug = blink_js_string;
  assets_[StaticAssetEnum::CLIENT_DOMAIN_REWRITER]->js_debug =
      JS_client_domain_rewriter;
  assets_[StaticAssetEnum::CRITICAL_CSS_BEACON_JS]->js_debug =
      JS_critical_css_beacon;
  assets_[StaticAssetEnum::CRITICAL_CSS_LOADER_JS]->js_debug =
      JS_critical_css_loader;
  assets_[StaticAssetEnum::CRITICAL_IMAGES_BEACON_JS]->js_debug =
      JS_critical_images_beacon;
  assets_[StaticAssetEnum::DEDUP_INLINED_IMAGES_JS]->js_debug =
      JS_dedup_inlined_images;
  assets_[StaticAssetEnum::DEFER_IFRAME]->js_debug = JS_defer_iframe;
  assets_[StaticAssetEnum::DEFER_JS]->js_debug = JS_js_defer;
  assets_[StaticAssetEnum::DELAY_IMAGES_JS]->js_debug = JS_delay_images;
  assets_[StaticAssetEnum::DELAY_IMAGES_INLINE_JS]->js_debug =
      JS_delay_images_inline;
  assets_[StaticAssetEnum::LAZYLOAD_IMAGES_JS]->js_debug = JS_lazyload_images;
  assets_[StaticAssetEnum::DETERMINISTIC_JS]->js_debug = JS_deterministic;
  // GhostClickBuster uses goog.require, which needs to be minifed always.
  assets_[StaticAssetEnum::GHOST_CLICK_BUSTER_JS]->js_debug =
      JS_ghost_click_buster_opt;
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->js_debug =
      JS_local_storage_cache;
  assets_[StaticAssetEnum::SPLIT_HTML_BEACON_JS]->js_debug =
      JS_split_html_beacon;

  // Initialize non-JS assets

  assets_[StaticAssetEnum::BLANK_GIF]->file_name = "1";
  assets_[StaticAssetEnum::BLANK_GIF]->js_optimized.append(
      reinterpret_cast<const char*>(GIF_blank), GIF_blank_len);
  assets_[StaticAssetEnum::BLANK_GIF]->js_debug.append(
      reinterpret_cast<const char*>(GIF_blank), GIF_blank_len);
  assets_[StaticAssetEnum::BLANK_GIF]->content_type = kContentTypeGif;

  for (std::vector<Asset*>::iterator it = assets_.begin();
       it != assets_.end(); ++it) {
    Asset* asset = *it;
    asset->js_opt_hash = hasher_->Hash(asset->js_optimized);
    asset->js_debug_hash = hasher_->Hash(asset->js_debug);

    // Make sure names are unique.
    DCHECK(file_name_to_module_map_.find(asset->file_name) ==
           file_name_to_module_map_.end())  << asset->file_name;
    // Setup a map of file name to the corresponding index in assets_ to
    // allow easier lookup in GetAsset.
    file_name_to_module_map_[asset->file_name] =
        static_cast<StaticAssetEnum::StaticAsset>(it - assets_.begin());
  }
  InitializeAssetUrls();
}

void StaticAssetManager::InitializeAssetUrls() {
  for (std::vector<Asset*>::iterator it = assets_.begin();
       it != assets_.end(); ++it) {
    Asset* asset = *it;
    // Generated urls are in the format "<filename>.<md5>.<extension>".
    asset->opt_url = StrCat(static_asset_base_,
                            library_url_prefix_,
                            asset->file_name,
                            ".", asset->js_opt_hash,
                            asset->content_type.file_extension());

    // Generated debug urls are in the format
    // "<filename>_debug.<md5>.<extension>".
    asset->debug_url = StrCat(static_asset_base_,
                              library_url_prefix_,
                              asset->file_name,
                              "_debug.", asset->js_debug_hash,
                              asset->content_type.file_extension());
  }
}

const char* StaticAssetManager::GetAsset(
    StaticAssetEnum::StaticAsset module, const RewriteOptions* options) const {
  CHECK(StaticAssetEnum::StaticAsset_IsValid(module));
  return options->Enabled(RewriteOptions::kDebug) ?
      assets_[module]->js_debug.c_str() :
      assets_[module]->js_optimized.c_str();
}

void StaticAssetManager::AddJsToElement(
    StringPiece js, HtmlElement* script, RewriteDriver* driver) const {
  DCHECK(script->keyword() == HtmlName::kScript);
  // CDATA tags are required for inlined JS in XHTML pages to prevent
  // interpretation of certain characters (like &). In apache, something
  // downstream of mod_pagespeed could modify the content type of the response.
  // So CDATA tags are added conservatively if we are not sure that it is safe
  // to exclude them.
  GoogleString js_str;

  if (!(driver->server_context()->response_headers_finalized() &&
        driver->MimeTypeXhtmlStatus() == RewriteDriver::kIsNotXhtml)) {
    StrAppend(&js_str, "//<![CDATA[\n", js, "\n//]]>");
    js = js_str;
  }

  if (!driver->doctype().IsVersion5()) {
    driver->AddAttribute(script, HtmlName::kType, "text/javascript");
  }
  HtmlCharactersNode* script_content = driver->NewCharactersNode(script, js);
  driver->AppendChild(script, script_content);
}

bool StaticAssetManager::GetAsset(StringPiece file_name,
                                  StringPiece* content,
                                  ContentType* content_type,
                                  StringPiece* cache_header) const {
  StringPieceVector names;
  SplitStringPieceToVector(file_name, ".", &names, true);
  // Expected file_name format is <name>[_debug].<HASH>.js
  // If file names doesn't contain hash in it, just return, because they may be
  // spurious request.
  if (names.size() != 3) {
    message_handler_->Message(kError, "Invalid url requested: %s.",
                              file_name.as_string().c_str());
    return false;
  }
  GoogleString plain_file_name;
  names[0].CopyToString(&plain_file_name);
  bool is_debug = false;

  if (StringPiece(plain_file_name).ends_with("_debug")) {
    is_debug = true;
    plain_file_name = plain_file_name.substr(0, plain_file_name.length() -
                                             strlen("_debug"));
  }

  FileNameToModuleMap::const_iterator p =
      file_name_to_module_map_.find(plain_file_name);
  if (p != file_name_to_module_map_.end()) {
    CHECK_GT(assets_.size(), static_cast<size_t>(p->second));
    Asset* asset = assets_[p->second];
    *content = is_debug ? asset->js_debug : asset->js_optimized;
    if (cache_header) {
      StringPiece hash = is_debug ? asset->js_debug_hash : asset->js_opt_hash;
      if (hash == names[1]) {  // compare hash
        *cache_header = cache_header_with_long_ttl_;
      } else {
        *cache_header = cache_header_with_private_ttl_;
      }
    }
    *content_type = asset->content_type;
    return true;
  }
  return false;
}

}  // namespace net_instaweb
