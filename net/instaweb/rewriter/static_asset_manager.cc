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
#include <utility>
#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
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
extern const char* JS_js_defer;
extern const char* JS_js_defer_opt;
extern const char* JS_lazyload_images;
extern const char* JS_lazyload_images_opt;
extern const char* JS_local_storage_cache;
extern const char* JS_local_storage_cache_opt;
extern const char* JS_responsive_js;
extern const char* JS_responsive_js_opt;

// TODO(jud): use the data2c build flow to create this data.
const unsigned char GIF_blank[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x1, 0x0, 0x1, 0x0,
    static_cast<unsigned char>(0x80), 0x0, 0x0,
    static_cast<unsigned char>(0xff), static_cast<unsigned char>(0xff),
    static_cast<unsigned char>(0xff), static_cast<unsigned char>(0xff),
    static_cast<unsigned char>(0xff), static_cast<unsigned char>(0xff), 0x21,
    static_cast<unsigned char>(0xfe), 0x6, 0x70, 0x73, 0x61, 0x5f, 0x6c, 0x6c,
    0x0, 0x21, static_cast<unsigned char>(0xf9), 0x4, 0x1, 0xa, 0x0, 0x1, 0x0,
    0x2c, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x2, 0x2, 0x4c, 0x1, 0x0,
    0x3b};
const int GIF_blank_len = arraysize(GIF_blank);

// The generated file js_defer.js is named in "<hash>-<fileName>" format.
const char StaticAssetManager::kGStaticBase[] =
    "//www.gstatic.com/psa/static/";

// TODO(jud): Change to "/psaassets/".
const char StaticAssetManager::kDefaultLibraryUrlPrefix[] = "/psajs/";

StaticAssetManager::StaticAssetManager(
    const GoogleString& static_asset_base,
    ThreadSystem* threads,
    Hasher* hasher,
    MessageHandler* message_handler)
    : static_asset_base_(static_asset_base),
      hasher_(hasher),
      message_handler_(message_handler),
      lock_(threads->NewRWLock()),
      serve_assets_from_gstatic_(false),
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
  ThreadSystem::ScopedReader read_lock(lock_.get());
  return options->Enabled(RewriteOptions::kDebug) ?
      assets_[module]->debug_url :
      assets_[module]->opt_url;
}

void StaticAssetManager::SetGStaticHashForTest(
    StaticAssetEnum::StaticAsset module, const GoogleString& hash) {
  CHECK(!hash.empty());
  StaticAssetConfig config;
  StaticAssetConfig::Asset* asset_conf = config.add_asset();
  asset_conf->set_role(module);
  {
    ThreadSystem::ScopedReader read_lock(lock_.get());  // read from assets_
    asset_conf->set_name(
        StrCat(assets_[module]->file_name,
               assets_[module]->content_type.file_extension()));
  }
  asset_conf->set_debug_hash(hash);
  asset_conf->set_opt_hash(hash);
  ApplyGStaticConfiguration(config, kInitialConfiguration);
}

void StaticAssetManager::ApplyGStaticConfiguration(
    const StaticAssetConfig& config,
    ConfigurationMode mode) {
  ScopedMutex write_lock(lock_.get());
  if (!serve_assets_from_gstatic_) {
    return;
  }
  if (mode == kInitialConfiguration) {
    initial_gstatic_config_.reset(new StaticAssetConfig);
    *initial_gstatic_config_ = config;
    ApplyGStaticConfigurationImpl(*initial_gstatic_config_,
                                  kInitialConfiguration);
  } else {
    // Apply initial + config.
    CHECK(initial_gstatic_config_.get() != NULL);
    StaticAssetConfig merged_config = *initial_gstatic_config_;
    merged_config.set_release_label(config.release_label());
    for (int i = 0, n = config.asset_size(); i < n; ++i) {
      const StaticAssetConfig::Asset& in = config.asset(i);
      StaticAssetConfig::Asset* out = merged_config.add_asset();
      *out = in;
    }
    ApplyGStaticConfigurationImpl(merged_config, kUpdateConfiguration);
  }
}

void StaticAssetManager::ResetGStaticConfiguration() {
  ScopedMutex write_lock(lock_.get());
  if (initial_gstatic_config_.get() != NULL) {
    // If there is no initial there is no update, so it's fine to do nothing
    // in the other case.
    ApplyGStaticConfigurationImpl(*initial_gstatic_config_,
                                  kInitialConfiguration);
  }
}

void StaticAssetManager::ApplyGStaticConfigurationImpl(
    const StaticAssetConfig& config, ConfigurationMode mode) {
  if (!serve_assets_from_gstatic_) {
    return;
  }

  for (int i = 0; i < config.asset_size(); ++i) {
    const StaticAssetConfig::Asset& asset_conf = config.asset(i);
    if (!StaticAssetEnum::StaticAsset_IsValid(asset_conf.role())) {
      LOG(DFATAL) << "Invalid asset role: " << asset_conf.role();
      return;
    }
    Asset* asset = assets_[asset_conf.role()];
    bool should_update = (mode == kInitialConfiguration) ||
                         (asset->release_label == config.release_label());
    if (should_update) {
      asset->opt_url = StrCat(gstatic_base_, asset_conf.opt_hash(), "-",
                              asset_conf.name());
      asset->debug_url = StrCat(gstatic_base_, asset_conf.debug_hash(), "-",
                                asset_conf.name());
      asset->release_label = config.release_label();
    }
  }
}

void StaticAssetManager::InitializeAssetStrings() {
  ScopedMutex write_lock(lock_.get());
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
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->file_name =
      "local_storage_cache";
  assets_[StaticAssetEnum::RESPONSIVE_JS]->file_name = "responsive";
  // Note that we still have to provide a name for these unused files because of
  // the DCHECK for unique names below.
  assets_[StaticAssetEnum::DEPRECATED_SPLIT_HTML_BEACON_JS]->file_name =
      "deprecated_split_html_beacon";
  assets_[StaticAssetEnum::DEPRECATED_GHOST_CLICK_BUSTER_JS]->file_name =
      "deprecated_ghost_click_buster";
  assets_[StaticAssetEnum::BLINK_JS]->file_name = "deprecated_blink";

  // Initialize compiled javascript strings->
  assets_[StaticAssetEnum::ADD_INSTRUMENTATION_JS]->js_optimized =
      JS_add_instrumentation_opt;
  assets_[StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS]->js_optimized =
      JS_extended_instrumentation_opt;
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
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->js_optimized =
      JS_local_storage_cache_opt;
  assets_[StaticAssetEnum::RESPONSIVE_JS]->js_optimized = JS_responsive_js_opt;

  // Initialize cleartext javascript strings->
  assets_[StaticAssetEnum::ADD_INSTRUMENTATION_JS]->js_debug =
      JS_add_instrumentation;
  assets_[StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS]->js_debug =
      JS_extended_instrumentation;
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
  assets_[StaticAssetEnum::LOCAL_STORAGE_CACHE_JS]->js_debug =
      JS_local_storage_cache;
  assets_[StaticAssetEnum::RESPONSIVE_JS]->js_debug = JS_responsive_js;

  // Initialize non-JS assets

  assets_[StaticAssetEnum::BLANK_GIF]->file_name = "1";
  assets_[StaticAssetEnum::BLANK_GIF]->js_optimized.append(
      reinterpret_cast<const char*>(GIF_blank), GIF_blank_len);
  assets_[StaticAssetEnum::BLANK_GIF]->js_debug.append(
      reinterpret_cast<const char*>(GIF_blank), GIF_blank_len);
  assets_[StaticAssetEnum::BLANK_GIF]->content_type = kContentTypeGif;

  assets_[StaticAssetEnum::MOBILIZE_JS]->file_name = nullptr;
  assets_[StaticAssetEnum::MOBILIZE_CSS]->file_name = nullptr;
  assets_[StaticAssetEnum::DEPRECATED_MOBILIZE_XHR_JS]->file_name = nullptr;
  assets_[StaticAssetEnum::DEPRECATED_MOBILIZE_LAYOUT_CSS]->file_name = nullptr;

  for (std::vector<Asset*>::iterator it = assets_.begin();
       it != assets_.end(); ++it) {
    Asset* asset = *it;
    if (asset->file_name == nullptr) {
      continue;
    }
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
  ThreadSystem::ScopedReader read_lock(lock_.get());
  CHECK(StaticAssetEnum::StaticAsset_IsValid(module));
  return options->Enabled(RewriteOptions::kDebug) ?
      assets_[module]->js_debug.c_str() :
      assets_[module]->js_optimized.c_str();
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

  ThreadSystem::ScopedReader read_lock(lock_.get());
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
