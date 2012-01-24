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

#include "net/instaweb/rewriter/public/rewrite_gflags.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

using namespace google;  // NOLINT

// This is used for prefixing file-based locks.
DEFINE_string(filename_prefix, "/tmp/instaweb/",
              "Filesystem prefix for storing resources.");

DEFINE_string(rewrite_level, "CoreFilters",
              "Base rewrite level. Must be one of: "
              "PassThrough, CoreFilters, TestingCoreFilters, AllFilters.");
DEFINE_string(rewriters, "", "Comma-separated list of rewriters");
DEFINE_string(domains, "", "Comma-separated list of domains");

DEFINE_int64(css_outline_min_bytes,
             net_instaweb::RewriteOptions::kDefaultCssOutlineMinBytes,
             "Number of bytes above which inline "
             "CSS resources will be outlined.");
DEFINE_int64(js_outline_min_bytes,
             net_instaweb::RewriteOptions::kDefaultJsOutlineMinBytes,
             "Number of bytes above which inline "
             "Javascript resources will be outlined.");
DEFINE_int64(image_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultImageInlineMaxBytes,
             "Number of bytes below which images will be inlined.");
DEFINE_int64(css_image_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultCssImageInlineMaxBytes,
             "Number of bytes below which images in CSS will be inlined.");
DEFINE_int32(image_jpeg_recompress_quality,
             net_instaweb::RewriteOptions::kDefaultImageJpegRecompressQuality,
             "Quality parameter to use while recompressing the jpeg images."
             "This should be in range [0,100], 100 refers to best quality.");
DEFINE_int32(
    image_limit_optimized_percent,
    net_instaweb::RewriteOptions::kDefaultImageLimitOptimizedPercent,
    "Optimized images will be used only if they are less than this percent "
    "size of the original image size.  100 retains any smaller image.");
DEFINE_int32(
    image_limit_resize_area_percent,
    net_instaweb::RewriteOptions::kDefaultImageLimitResizeAreaPercent,
    "Only attempt to shrink an image on the server if its area is less than "
    "this percent of the original image area.  100 always shrinks the image "
    "if its dimensions are smaller.");
DEFINE_int64(js_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultJsInlineMaxBytes,
             "Number of bytes below which javascript will be inlined.");
DEFINE_int64(css_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultCssInlineMaxBytes,
             "Number of bytes below which stylesheets will be inlined.");
DEFINE_int32(image_max_rewrites_at_once,
             net_instaweb::RewriteOptions::kDefaultImageMaxRewritesAtOnce,
             "Maximum number of images that will be rewritten simultaneously.");
DEFINE_bool(ajax_rewriting_enabled, false, "Boolean to indicate whether ajax "
            "rewriting is enabled.");
DEFINE_bool(log_rewrite_timing, false, "Log time taken by rewrite filters.");

DEFINE_int64(max_html_cache_time_ms,
             net_instaweb::RewriteOptions::kDefaultMaxHtmlCacheTimeMs,
             "Default Cache-Control TTL for HTML. "
             "Cache-Control TTL will be set to the lower of this value "
             "and the original TTL.");
DEFINE_int64(
    min_resource_cache_time_to_rewrite_ms,
    net_instaweb::RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs,
    "No resources with Cache-Control TTL less than this will be rewritten.");

DEFINE_string(origin_domain_map, "",
              "Semicolon-separated list of origin_domain maps. "
              "Each domain-map is of the form dest=src1,src2,src3");
DEFINE_string(rewrite_domain_map, "",
              "Semicolon-separated list of rewrite_domain maps. "
              "Each domain-map is of the form dest=src1,src2,src3");
DEFINE_string(shard_domain_map, "",
              "Semicolon-separated list of shard_domain maps. "
              "Each domain-map is of the form master=shard1,shard2,shard3");

DEFINE_int32(lru_cache_size_bytes, 10 * 1000 * 1000, "LRU cache size");
DEFINE_bool(force_caching, false,
            "Ignore caching headers and cache everything.");
DEFINE_bool(flush_html, false, "Pass fetcher-generated flushes through HTML");
DEFINE_bool(serve_stale_if_fetch_error, true, "Serve stale content if the "
            "fetch results in an error.");
DEFINE_int32(psa_idle_flush_time_ms,
             net_instaweb::RewriteOptions::kDefaultIdleFlushTimeMs,
             "If the input HTML stops coming in for this many ms, a flush"
             " will be injected. Use a value <= 0 to disable.");
DEFINE_string(pagespeed_version, "", "Version number to put into X-Page-Speed "
              "response header.");
DEFINE_bool(enable_blink, false, "If true then blink is enabled");

// TODO(pulkitg): Add following flags in apache/mod_instaweb.cc
DEFINE_int32(max_delayed_images_index,
             net_instaweb::RewriteOptions::kDefaultMaxDelayedImagesIndex,
             "Number of first N images for which low res image is generated. "
             "Negative values will bypass image index check.");

DEFINE_int64(min_image_size_low_resolution_bytes,
    net_instaweb::RewriteOptions::kDefaultMinImageSizeLowResolutionBytes,
    "Minimum image size above which low res image is generated.");

DEFINE_int64(critical_images_cache_expiration_time_ms,
    net_instaweb::RewriteOptions::kDefaultCriticalImagesCacheExpirationMs,
    "Critical images ajax metadata cache expiration time in msec.");

DEFINE_int64(
    metadata_cache_staleness_threshold_ms,
    net_instaweb::RewriteOptions::kDefaultMetadataCacheStalenessThresholdMs,
    "Maximum time in milliseconds beyond expiry for which a metadata cache "
    "entry may be used in milliseconds.");

namespace net_instaweb {

namespace {

#define CALL_MEMBER_FN(object, var) (object->*(var))

bool AddDomainMap(const StringPiece& flag_value, DomainLawyer* lawyer,
                  bool (DomainLawyer::*fn)(const StringPiece& to_domain,
                                           const StringPiece& from,
                                           MessageHandler* handler),
                  MessageHandler* message_handler) {
  bool ret = true;
  StringPieceVector maps;
  // split "a=b,c,d=e:g,f" by semicolons.
  SplitStringPieceToVector(flag_value, ";", &maps, true);
  for (int i = 0, n = maps.size(); i < n; ++i) {
    // parse "a=b,c,d" into "a" and "b,c,d"
    StringPieceVector name_values;
    SplitStringPieceToVector(maps[i], "=", &name_values, true);
    if (name_values.size() != 2) {
      message_handler->Message(kError, "Invalid rewrite_domain_map: %s",
                               maps[i].as_string().c_str());
      ret = false;
    } else {
      ret &= CALL_MEMBER_FN(lawyer, fn)(name_values[0], name_values[1],
                                        message_handler);
    }
  }
  return ret;
}

}  // namespace

RewriteGflags::RewriteGflags(const char* progname, int* argc, char*** argv) {
  ParseCommandLineFlags(argc, argv, true);
}

bool RewriteGflags::SetOptions(RewriteDriverFactory* factory,
                               RewriteOptions* options) const {
  bool ret = true;
  factory->set_filename_prefix(FLAGS_filename_prefix);
  factory->set_force_caching(FLAGS_force_caching);
  factory->set_version_string(FLAGS_pagespeed_version);

  if (WasExplicitlySet("css_outline_min_bytes")) {
    options->set_css_outline_min_bytes(FLAGS_css_outline_min_bytes);
  }
  if (WasExplicitlySet("js_outline_min_bytes")) {
    options->set_js_outline_min_bytes(FLAGS_js_outline_min_bytes);
  }
  if (WasExplicitlySet("image_inline_max_bytes")) {
    options->set_image_inline_max_bytes(FLAGS_image_inline_max_bytes);
  }
  if (WasExplicitlySet("css_image_inline_max_bytes")) {
    options->set_css_image_inline_max_bytes(FLAGS_css_image_inline_max_bytes);
  }
  if (WasExplicitlySet("css_inline_max_bytes")) {
    options->set_css_inline_max_bytes(FLAGS_css_inline_max_bytes);
  }
  if (WasExplicitlySet("js_inline_max_bytes")) {
    options->set_js_inline_max_bytes(FLAGS_js_inline_max_bytes);
  }
  if (WasExplicitlySet("image_max_rewrites_at_once")) {
    options->set_image_max_rewrites_at_once(
        FLAGS_image_max_rewrites_at_once);
  }

  if (WasExplicitlySet("log_rewrite_timing")) {
    options->set_log_rewrite_timing(FLAGS_log_rewrite_timing);
  }
  if (WasExplicitlySet("max_html_cache_time_ms")) {
    options->set_max_html_cache_time_ms(FLAGS_max_html_cache_time_ms);
  }
  if (WasExplicitlySet("min_resource_cache_time_to_rewrite_ms")) {
    options->set_min_resource_cache_time_to_rewrite_ms(
        FLAGS_min_resource_cache_time_to_rewrite_ms);
  }
  if (WasExplicitlySet("flush_html")) {
    options->set_flush_html(FLAGS_flush_html);
  }
  if (WasExplicitlySet("serve_stale_if_fetch_error")) {
    options->set_serve_stale_if_fetch_error(FLAGS_serve_stale_if_fetch_error);
  }
  if (WasExplicitlySet("psa_idle_flush_time_ms")) {
    options->set_idle_flush_time_ms(FLAGS_psa_idle_flush_time_ms);
  }
  if (WasExplicitlySet("image_jpeg_recompress_quality")) {
    options->set_image_jpeg_recompress_quality(
        FLAGS_image_jpeg_recompress_quality);
  }
  if (WasExplicitlySet("image_limit_optimized_percent")) {
    options->set_image_limit_optimized_percent(
        FLAGS_image_limit_optimized_percent);
  }
  if (WasExplicitlySet("image_limit_resize_area_percent")) {
    options->set_image_limit_resize_area_percent(
        FLAGS_image_limit_resize_area_percent);
  }
  if (WasExplicitlySet("enable_blink")) {
    options->set_enable_blink(FLAGS_enable_blink);
  }
  if (WasExplicitlySet("max_delayed_images_index")) {
    options->set_max_delayed_images_index(FLAGS_max_delayed_images_index);
  }
  if (WasExplicitlySet("min_image_size_low_resolution_bytes")) {
    options->set_min_image_size_low_resolution_bytes(
        FLAGS_min_image_size_low_resolution_bytes);
  }
  if (WasExplicitlySet("critical_images_cache_expiration_time_ms")) {
    options->set_critical_images_cache_expiration_time_ms(
        FLAGS_critical_images_cache_expiration_time_ms);
  }
  if (WasExplicitlySet("metadata_cache_staleness_threshold_ms")) {
    options->set_metadata_cache_staleness_threshold_ms(
        FLAGS_metadata_cache_staleness_threshold_ms);
  }

  // TODO(nikhilmadan): Check if this is explicitly set. Since this has been
  // disabled by default because of potential conflicts with Apache, we are
  // forcing this to be set in the default options.
  options->set_ajax_rewriting_enabled(FLAGS_ajax_rewriting_enabled);

  MessageHandler* handler = factory->message_handler();

  StringPieceVector domains;
  SplitStringPieceToVector(FLAGS_domains, ",", &domains, true);
  DomainLawyer* lawyer = options->domain_lawyer();
  for (int i = 0, n = domains.size(); i < n; ++i) {
    if (!lawyer->AddDomain(domains[i], handler)) {
      LOG(ERROR) << "Invalid domain: " << domains[i];
      ret = false;
    }
  }

  if (WasExplicitlySet("rewrite_domain_map")) {
    ret &= AddDomainMap(FLAGS_rewrite_domain_map, lawyer,
                        &DomainLawyer::AddRewriteDomainMapping, handler);
  }

  if (WasExplicitlySet("shard_domain_map")) {
    ret &= AddDomainMap(FLAGS_shard_domain_map, lawyer,
                        &DomainLawyer::AddShard, handler);
  }

  if (WasExplicitlySet("origin_domain_map")) {
    ret &= AddDomainMap(FLAGS_origin_domain_map, lawyer,
                        &DomainLawyer::AddOriginDomainMapping, handler);
  }

  ret &= SetRewriters("rewriters", FLAGS_rewriters.c_str(),
                      "rewrite_level", FLAGS_rewrite_level.c_str(),
                      options, handler);
  return ret;
}

// prefix with "::" is needed because of 'using namespace google' above.
::int64 RewriteGflags::lru_cache_size_bytes() const {
  return FLAGS_lru_cache_size_bytes;
}

bool RewriteGflags::WasExplicitlySet(const char* name) const {
  CommandLineFlagInfo flag_info;
  CHECK(GetCommandLineFlagInfo(name, &flag_info));
  return !flag_info.is_default;
}

bool RewriteGflags::SetRewriters(const char* rewriters_flag_name,
                                 const char* rewriters_value,
                                 const char* rewrite_level_flag_name,
                                 const char* rewrite_level_value,
                                 RewriteOptions* options,
                                 MessageHandler* handler) const {
  bool ret = true;
  RewriteOptions::RewriteLevel rewrite_level;
  if (options->ParseRewriteLevel(rewrite_level_value, &rewrite_level)) {
    options->SetRewriteLevel(rewrite_level);
  } else {
    LOG(ERROR) << "Invalid --" << rewrite_level_flag_name
               << ": " << rewrite_level_value;
    ret = false;
  }

  if (!options->EnableFiltersByCommaSeparatedList(rewriters_value, handler)) {
    LOG(ERROR) << "Invalid --" << rewriters_flag_name
               << ": " << rewriters_value;
    ret = false;
  }
  return ret;
}

}  // namespace net_instaweb
