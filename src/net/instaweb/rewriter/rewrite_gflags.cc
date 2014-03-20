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

#include <memory>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

using google::CommandLineFlagInfo;
using google::GetCommandLineFlagInfo;

using net_instaweb::RewriteOptions;

// This is used for prefixing file-based locks.
DEFINE_string(filename_prefix, "/tmp/instaweb/",
              "Filesystem prefix for storing resources.");

DEFINE_string(rewrite_level, "CoreFilters",
              "Base rewrite level. Must be one of: "
              "PassThrough, CoreFilters, TestingCoreFilters, AllFilters.");
DEFINE_string(rewriters, "", "Comma-separated list of rewriters");
// distributable_filters is for experimentation and may be removed later.
DEFINE_string(distributable_filters, "",
    "List of comma-separated filters whose rewrites should be distributed to "
    "another task.");
DEFINE_string(domains, "", "Comma-separated list of domains");

DEFINE_int64(css_outline_min_bytes,
             RewriteOptions::kDefaultCssOutlineMinBytes,
             "Number of bytes above which inline "
             "CSS resources will be outlined.");
DEFINE_int64(js_outline_min_bytes,
             RewriteOptions::kDefaultJsOutlineMinBytes,
             "Number of bytes above which inline "
             "Javascript resources will be outlined.");
DEFINE_int64(image_inline_max_bytes,
             RewriteOptions::kDefaultImageInlineMaxBytes,
             "Number of bytes below which images will be inlined.");
DEFINE_int64(css_image_inline_max_bytes,
             RewriteOptions::kDefaultCssImageInlineMaxBytes,
             "Number of bytes below which images in CSS will be inlined.");
DEFINE_int32(image_recompress_quality,
             RewriteOptions::kDefaultImageRecompressQuality,
             "Quality parameter to use while recompressing any image type. "
             "This should be in the range [0,100]. 100 refers to the highest "
             "quality.");
// Deprecated flag.
DEFINE_int32(images_recompress_quality, -1,
             "DEPRECATED.  Does nothing.  Use image_recompress_quality.");
DEFINE_int32(image_jpeg_recompress_quality,
             RewriteOptions::kDefaultImageJpegRecompressQuality,
             "Quality parameter to use while recompressing jpeg images. "
             "This should be in the range [0,100]. 100 refers to the highest "
             "quality.");
DEFINE_int32(image_jpeg_recompress_quality_for_small_screens,
             RewriteOptions::kDefaultImageJpegRecompressQualityForSmallScreens,
             "Quality parameter to use while recompressing jpeg images "
             "for small screens. This should be in the range [0,100]. 100 "
             "refers to the highest quality. If -1 or not set, "
             "image_jpeg_recompress_quality will be used.");
DEFINE_int32(image_jpeg_num_progressive_scans,
             RewriteOptions::kDefaultImageJpegNumProgressiveScans,
             "The number of scans, [0,10] to serve for jpeg images that "
             "are encoded as ten-scan progressive jpegs.");
DEFINE_int32(image_jpeg_num_progressive_scans_for_small_screens,
             RewriteOptions::kDefaultImageJpegNumProgressiveScans,
             "The number of scans, [0,10], to serve to small-screen devices "
             "for jpeg images that are encoded as ten-scan progressive jpegs.");
DEFINE_int32(image_webp_recompress_quality,
             RewriteOptions::kDefaultImageWebpRecompressQuality,
             "Quality parameter to use while recompressing webp images. "
             "This should be in the range [0,100]. 100 refers to the highest "
             "quality.");
DEFINE_int32(image_webp_recompress_quality_for_small_screens,
             RewriteOptions::kDefaultImageWebpRecompressQualityForSmallScreens,
             "Quality parameter to use while recompressing webp images "
             "for small screens. This should be in the range [0,100]. 100 "
             "refers to the highest quality. If -1 or not set, "
             "image_webp_recompress_quality will be used.");
DEFINE_int64(image_webp_timeout_ms,
             RewriteOptions::kDefaultImageWebpTimeoutMs,
             "The timeout, in milliseconds, for converting images to WebP "
             "format. A negative value means 'no timeout'.");
DEFINE_int32(
    image_limit_optimized_percent,
    RewriteOptions::kDefaultImageLimitOptimizedPercent,
    "Optimized images will be used only if they are less than this percent "
    "size of the original image size.  100 retains any smaller image.");
DEFINE_int32(
    image_limit_rendered_area_percent,
    RewriteOptions::kDefaultImageLimitRenderedAreaPercent,
    "Store image's rendered dimensions into property cache only if the "
    "rendered dimensions reduce the image area more than the limit");
DEFINE_int32(
    image_limit_resize_area_percent,
    RewriteOptions::kDefaultImageLimitResizeAreaPercent,
    "Only attempt to shrink an image on the server if its area is less than "
    "this percent of the original image area.  100 always shrinks the image "
    "if its dimensions are smaller.");
DEFINE_int64(js_inline_max_bytes,
             RewriteOptions::kDefaultJsInlineMaxBytes,
             "Number of bytes below which javascript will be inlined.");
DEFINE_int64(css_flatten_max_bytes,
             RewriteOptions::kDefaultCssFlattenMaxBytes,
             "Number of bytes below which stylesheets will be flattened.");
DEFINE_int64(css_inline_max_bytes,
             RewriteOptions::kDefaultCssInlineMaxBytes,
             "Number of bytes below which stylesheets will be inlined.");
DEFINE_int32(image_max_rewrites_at_once,
             RewriteOptions::kDefaultImageMaxRewritesAtOnce,
             "Maximum number of images that will be rewritten simultaneously.");
DEFINE_bool(ajax_rewriting_enabled, false, "Deprecated. Use "
            "in_place_rewriting_enabled.");
DEFINE_bool(in_place_rewriting_enabled, false, "Boolean to indicate whether "
            "in-place-resource-optimization(IPRO) is enabled.");
DEFINE_bool(in_place_wait_for_optimized, false, "Indicates whether in-place "
            "resource optimization should wait to optimize the resource before "
            "responding.");
DEFINE_int32(in_place_rewrite_deadline_ms,
             RewriteOptions::kDefaultRewriteDeadlineMs,
             "Deadline for rewriting a resource on the in-place serving path. "
             "(--in_place_wait_for_optimized must be set for this to apply.) "
             "After this interval passes, the original unoptimized resource "
             "will be served to clients. A value of -1 will wait indefinitely "
             "for each in-place rewrite to complete.");
// TODO(piatek): Add external docs for these.
DEFINE_bool(in_place_preemptive_rewrite_css, true,
            "If set, issue preemptive rewrites of CSS on the HTML path when "
            "configured to use IPRO. If --css_preserve_urls is not set, this "
            "has no effect.");
DEFINE_bool(in_place_preemptive_rewrite_css_images, true,
            "If set, preemptive rewrite images in CSS files on the IPRO "
            "serving path.");
DEFINE_bool(in_place_preemptive_rewrite_images, true,
            "If set, issue preemptive rewrites of images on the HTML path when "
            "configured to use IPRO. If --image_preserve_urls is not set, this "
            "flag has no effect.");
DEFINE_bool(in_place_preemptive_rewrite_javascript, true,
            "If set, issue preemptive rewrites of javascript on the HTML path "
            "when configured to use IPRO. If --js_preserve_urls is not set, "
            "this flag has no effect.");
DEFINE_bool(private_not_vary_for_ie, true,
            "If set, use Cache-Control: private rather than Vary: Accept when "
            "serving IPRO resources to IE.  This avoids the need for an "
            "if-modified-since request from IE, but prevents proxy caching of "
            "these resources.");
DEFINE_bool(image_preserve_urls, false, "Boolean to indicate whether image"
            "URLs should be preserved.");
DEFINE_bool(css_preserve_urls, false, "Boolean to indicate whether CSS URLS"
            "should be preserved.");
DEFINE_bool(js_preserve_urls, false, "Boolean to indicate whether JavaScript"
            "URLs should be preserved.");
DEFINE_int32(rewrite_deadline_per_flush_ms,
             RewriteOptions::kDefaultRewriteDeadlineMs,
             "Deadline to rewrite a resource before putting the rewrite in the "
             "background and returning the original resource. A value of -1 "
             "will result in waiting for all rewrites to complete.");
DEFINE_int32(
    experiment_cookie_duration_ms,
    RewriteOptions::kDefaultExperimentCookieDurationMs,
    "Duration after which the experiment cookie used for A/B experiments "
    "should expire on the user's browser.");
DEFINE_bool(log_background_rewrites, false,
            "Log rewriting that cannot finish in the line-of-request.");
DEFINE_bool(log_rewrite_timing, false, "Log time taken by rewrite filters.");
DEFINE_bool(log_url_indices, false, "Log URL indices for every rewriter "
            "application.");
DEFINE_int64(max_html_cache_time_ms,
             RewriteOptions::kDefaultMaxHtmlCacheTimeMs,
             "Default Cache-Control TTL for HTML. "
             "Cache-Control TTL will be set to the lower of this value "
             "and the original TTL.");
DEFINE_int64(
    min_resource_cache_time_to_rewrite_ms,
    RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs,
    "No resources with Cache-Control TTL less than this will be rewritten.");
DEFINE_bool(
    hide_referer_using_meta,
    false, "Hide referer by adding meta tag to the HTML.");
DEFINE_int64(
    max_low_res_image_size_bytes,
    RewriteOptions::kDefaultMaxLowResImageSizeBytes,
    "Maximum size (in bytes) of the low resolution image which will be "
    "inline-previewed in html.");
DEFINE_int32(
    max_low_res_to_full_res_image_size_percentage,
    RewriteOptions::kDefaultMaxLowResToFullResImageSizePercentage,
    "The maximum ratio of the size of low-res image to its full-res image "
    "when the image will be inline-previewed in html. This is an integer "
    "ranging from 0 to 100 (inclusive).");

DEFINE_string(origin_domain_map, "",
              "Semicolon-separated list of origin_domain maps. "
              "Each domain-map is of the form dest=src1,src2,src3");
DEFINE_string(rewrite_domain_map, "",
              "Semicolon-separated list of rewrite_domain maps. "
              "Each domain-map is of the form dest=src1,src2,src3");
DEFINE_string(shard_domain_map, "",
              "Semicolon-separated list of shard_domain maps. "
              "Each domain-map is of the form master=shard1,shard2,shard3");

DEFINE_int64(lru_cache_size_bytes, 10 * 1024 * 1024, "LRU cache size");
DEFINE_bool(force_caching, false,
            "Ignore caching headers and cache everything.");
DEFINE_bool(flush_html, false, "Pass fetcher-generated flushes through HTML");
// TODO(pulkig): Remove proactively_freshen_user_facing_request flag after
// testing is done.
DEFINE_bool(proactively_freshen_user_facing_request, false,
            "Proactively freshen user facing requests if they are about to"
            "expire in cache.");
DEFINE_bool(serve_split_html_in_two_chunks, false,
            "Whether to serve the split html response in two chunks.");
DEFINE_bool(serve_stale_if_fetch_error, true, "Serve stale content if the "
            "fetch results in an error.");
DEFINE_int64(serve_stale_while_revalidate_threshold_sec, 0, "Threshold for "
            "serving stale content while revalidating in background."
            "0 means don't serve stale content."
            "Note: Stale response will be served only for non-html requests.");
DEFINE_int32(psa_flush_buffer_limit_bytes,
             RewriteOptions::kDefaultFlushBufferLimitBytes,
             "Whenever more than this much HTML gets buffered, a flush"
             "will be injected.");
DEFINE_int32(psa_idle_flush_time_ms,
             RewriteOptions::kDefaultIdleFlushTimeMs,
             "If the input HTML stops coming in for this many ms, a flush"
             " will be injected. Use a value <= 0 to disable.");
DEFINE_string(pagespeed_version, "", "Version number to put into X-Page-Speed "
              "response header.");
DEFINE_bool(enable_flush_early_critical_css, false,
            "If true, inlined critical css rules are flushed early if both"
            "flush subresources and critical css filter are enabled");
DEFINE_bool(use_selectors_for_critical_css, false,
            "Use CriticalSelectorFilter instead of CriticalCssFilter for "
            "the prioritize_critical_css filter.");
DEFINE_int32(max_inlined_preview_images_index,
             RewriteOptions::kDefaultMaxInlinedPreviewImagesIndex,
             "Number of first N images for which low res image is generated. "
             "Negative values will bypass image index check.");

DEFINE_int64(min_image_size_low_resolution_bytes,
    RewriteOptions::kDefaultMinImageSizeLowResolutionBytes,
    "Minimum image size above which low res image is generated.");

DEFINE_int64(max_image_size_low_resolution_bytes,
    RewriteOptions::kDefaultMaxImageSizeLowResolutionBytes,
    "Maximum image size below which low res image is generated.");

DEFINE_int64(finder_properties_cache_expiration_time_ms,
    RewriteOptions::kDefaultFinderPropertiesCacheExpirationTimeMs,
    "Cache expiration time for properties of finders in msec.");

DEFINE_int64(finder_properties_cache_refresh_time_ms,
    RewriteOptions::kDefaultFinderPropertiesCacheRefreshTimeMs,
    "Cache refresh time for properties of finders in msec.");

DEFINE_int64(
    metadata_cache_staleness_threshold_ms,
    RewriteOptions::kDefaultMetadataCacheStalenessThresholdMs,
    "Maximum time in milliseconds beyond expiry for which a metadata cache "
    "entry may be used in milliseconds.");

DEFINE_bool(lazyload_images_after_onload, false, "Boolean indicating whether "
            "lazyload images should load images when onload is fired. If "
            "false, images are loaded onscroll.");

DEFINE_bool(use_blank_image_for_inline_preview, false, "Boolean to decide "
            "whether to use blank image for inline preview or a low resolution "
            "version of the original image.");

DEFINE_bool(use_fallback_property_cache_values, false,
            "Boolean indicating whether to use fallback property cache "
            "values(i.e. without query params) in case actual values are not "
            "available.");

DEFINE_bool(await_pcache_lookup, false,
            "Boolean indicating whether to always wait for property cache "
            "lookup to finish.");

DEFINE_string(lazyload_images_blank_url, "",
              "The initial image url to load in the lazyload images filter.");

DEFINE_string(pre_connect_url, "",
              "Url to which pre connect requests will be sent.");

DEFINE_bool(inline_only_critical_images, true, "Boolean indicating whether "
            "inline_images should inline only critical images or not.");

DEFINE_bool(critical_images_beacon_enabled, false, "If beaconing is enabled "
            "via --use_beacon_results_in_filters, then this indicates that the "
            "critical images beacon should be inserted for image rewriting.");

DEFINE_bool(test_only_prioritize_critical_css_dont_apply_original_css, false,
            "Boolean indicating whether the prioritize_critical_css filter "
            "should invoke its JavaScript function to load all 'hidden' CSS "
            "at onload.");

DEFINE_int64(implicit_cache_ttl_ms,
             RewriteOptions::kDefaultImplicitCacheTtlMs,
             "The number of milliseconds of cache TTL we assign to resources "
             "that are likely cacheable (e.g. images, js, css, not html) and "
             "have no explicit cache ttl or expiration date.");

DEFINE_int64(min_cache_ttl_ms,
             RewriteOptions::kDefaultMinCacheTtlMs,
             "The minimum milliseconds of cache TTL for all resources that "
             "are explicitly cacheable. This overrides the max-age even when "
             "it is set on the Cache-Control headers.");

DEFINE_int32(property_cache_http_status_stability_threshold,
             RewriteOptions::kDefaultPropertyCacheHttpStatusStabilityThreshold,
             "The number of requests for which the status code should remain "
             "same so that we consider it to be stable.");

DEFINE_int32(max_prefetch_js_elements,
             RewriteOptions::kDefaultMaxPrefetchJsElements,
             "The number of JS elements to prefetch and download when defer "
             "Javascript filter is enabled.");

DEFINE_bool(enable_defer_js_experimental, false,
            "Enables experimental defer js.");

DEFINE_bool(disable_background_fetches_for_bots, false,
            "Disable pre-emptive background fetches on bot requests.");

DEFINE_bool(disable_rewrite_on_no_transform, true,
            "Disable any rewrite optimizations if the header contains "
            "no-transform cache-control.");

DEFINE_bool(lazyload_highres_images, false,
            "Enables experimental lazy load of high res images.");

DEFINE_bool(flush_more_resources_early_if_time_permits, false,
            "Flush more resources if origin is slow to respond.");

DEFINE_bool(flush_more_resources_in_ie_and_firefox, false,
            "Flush more resources if origin is slow to respond in IE and "
            "Firefox.");

DEFINE_bool(avoid_renaming_introspective_javascript, true,
            "Don't combine, inline, cache extend, or otherwise modify "
            "javascript in ways that require changing the URL if we see "
            "introspection in the form of "
            "document.getElementsByTagName('script').");

DEFINE_string(known_libraries, "",
              "Metadata about known libraries, formatted as bytes md5 url.  "
              "May contain multiple space-separated entries: "
              "--known_libraries=\"105527 ltVVzzYxo0 "
              "//ajax.googleapis.com/ajax/libs/1.6.1.0/prototype.js  "
              "92501 J8KF47pYOq "
              "//ajax.googleapis.com/ajax/libs/jquery/1.8.0/jquery.min.js\"  "
              "Obtain entry data by running "
              "net/instaweb/rewriter/js_minify --print_size_and_hash "
              "library.js");

DEFINE_string(experiment_specs, "",
              "A '+'-separated list of experiment_specs. For example "
              "'id=7;enable=recompress_images;percent=50+id=2;enable="
              "recompress_images,convert_jpeg_to_progressive;percent=5'.");

DEFINE_bool(enable_cache_purge, false, "Enables the Cache Purge API. This "
            "requires saving input URLs to each metadata cache entry to "
            "facilitate fast URL cache invalidation.");

DEFINE_bool(proactive_resource_freshening, false, "If set, the URLs of the "
            "inputs to the optimization are saved in the metadata cache entry. "
            "This is used to freshen resources when they are close to expiry.");

DEFINE_bool(enable_extended_instrumentation, false,
            "If set to true, additional instrumentation js added to that "
            "page that adds more information to the beacon.");

DEFINE_bool(use_experimental_js_minifier, false,
            "If set to true, uses the new JsTokenizer-based minifier. "
            "This option will be removed when that minifier has matured.");

DEFINE_string(blocking_rewrite_key,
              RewriteOptions::kDefaultBlockingRewriteKey,
              "Enables rewrites to finish before the response is sent to "
              "the client, if X-PSA-Blocking-Rewrite http request header's "
              "value is same as this flag's value.");

DEFINE_string(distributed_rewrite_key, "",
              "The user-provided key used to authenticate requests from one "
              "rewrite task to another.  Right now only used to validate "
              "meta-data headers in distributed rewrites in the html path. "
              "This should be random, greater than 8 characters, and the same "
              "value for each task. An empty value disables features that rely "
              "on it.");

DEFINE_string(distributed_rewrite_servers, "",
              "Comma-separated list of servers for distributed rewriting. "
              "Servers can be BNS jobs or host:port pairs.");

DEFINE_int64(distributed_rewrite_timeout_ms,
             RewriteOptions::kDefaultDistributedTimeoutMs,
             "Time to wait for a distributed rewrite to complete before "
             "abandoning it.");

DEFINE_bool(
    distribute_fetches, true,
    "Whether or not to distribute IPRO and .pagespeed. resource fetch requests "
    "from the RewriteDriver before checking the cache.");

DEFINE_bool(support_noscript_enabled, true,
            "Support for clients with no script support, in filters that "
            "insert new javascript.");

DEFINE_bool(enable_blink_debug_dashboard, true,
            "Enable blink dashboard used for debugging.");

DEFINE_bool(report_unload_time, false, "If enabled, sends beacons when page "
            "unload happens before onload.");

DEFINE_int64(max_combined_css_bytes, -1,
            "Maximum size allowed for the combined CSS resource. "
            "Negative values will bypass size check.");

DEFINE_int64(max_combined_js_bytes, -1,
            "Maximum size allowed for the combined js resource. "
            "Negative values will bypass size check.");

DEFINE_int64(
    blink_html_change_detection_time_ms,
    RewriteOptions::kDefaultBlinkHtmlChangeDetectionTimeMs,
    "Time after which we should try to detect if publisher html has changed");

DEFINE_bool(enable_blink_html_change_detection, false,
            "If enabled automatically detect publisher changes in html in "
            "blink.");

DEFINE_bool(enable_blink_html_change_detection_logging, false,
            "If enabled, html change detection is applied to all blink sites"
            " and the results are logged. Critical line recomputation is not"
            " triggered in case of mismatch.");

DEFINE_bool(use_smart_diff_in_blink, false,
            "If enabled use smart diff to detect publisher changes in html "
            "in blink");

DEFINE_bool(persist_blink_blacklist, false,
            "Persist the blink blacklist by writing to kansas.");

DEFINE_bool(enable_prioritizing_scripts, false,
    "If it is set to true, defer javascript will prioritize scripts with"
    "data-pagespeed-prioritize attibute.");

DEFINE_int64(max_image_bytes_for_webp_in_css,
             RewriteOptions::kDefaultMaxImageBytesForWebpInCss,
             "The maximum size of an image in CSS, which we convert to webp.");

DEFINE_bool(override_ie_document_mode, false,
            "If enabled, IE will be made to use the highest mode available"
            " to that version of IE.");

DEFINE_int64(max_html_parse_bytes,
             RewriteOptions::kDefaultMaxHtmlParseBytes,
             "The maximum number of bytes in a html that we parse before "
             "redirecting to a page with no rewriting.");

DEFINE_int64(
    metadata_input_errors_cache_ttl_ms,
    RewriteOptions::kDefaultMetadataInputErrorsCacheTtlMs,
    "The metadata cache ttl for input resources which are 4xx errors.");

DEFINE_bool(enable_aggressive_rewriters_for_mobile, false,
            "If true then aggressive rewriters will be turned on for "
            "mobile user agents.");

DEFINE_string(lazyload_disabled_classes, "",
              "A comma separated list of classes for which the lazyload images "
              "filter is disabled.");

DEFINE_int32(max_rewrite_info_log_size,
             RewriteOptions::kDefaultMaxRewriteInfoLogSize,
             "The maximum number of RewriterInfo submessage stored for a "
             "single request.");

DEFINE_bool(oblivious_pagespeed_urls, false,
            "If set to true, the system will retrieve .pagespeed. resources, "
            "instead of decoding and retrieving original. Should be set only "
            "for IPRO PreserveUrls.");

DEFINE_bool(rewrite_uncacheable_resources, false,
            "If set to true, the system will rewrite all the resources in "
            "in-place rewriting mode, regardless of resource's caching "
            "settings. in_place_wait_for_optimized flag should also be set.");

DEFINE_bool(serve_ghost_click_buster_with_split_html, false,
            "Whether ghost click buster code is served along with split_html.");

DEFINE_bool(serve_xhr_access_control_headers, false,
            "If set to true, adds access control headers to response headers.");

DEFINE_string(access_control_allow_origins, "",
              "Comma seperated list of origins that are allowed to make "
              "cross-origin requests. These domain requests are served with "
              "Access-Control-Allow-Origin header.");

namespace net_instaweb {

namespace {

bool DomainMapRewriteDomain(DomainLawyer* lawyer,
                            const StringPiece& to_domain,
                            const StringPiece& from,
                            MessageHandler* handler) {
  return lawyer->AddRewriteDomainMapping(to_domain, from, handler);
}

bool DomainMapOriginDomain(DomainLawyer* lawyer,
                           const StringPiece& to_domain,
                           const StringPiece& from,
                           MessageHandler* handler) {
  // Note that we don't currently have a syntax to specify a Host header
  // from flags.  This can be created as the need arises.
  return lawyer->AddOriginDomainMapping(to_domain, from, "" /* host_header */,
                                        handler);
}

bool DomainAddShard(DomainLawyer* lawyer,
                            const StringPiece& to_domain,
                            const StringPiece& from,
                            MessageHandler* handler) {
  return lawyer->AddShard(to_domain, from, handler);
}

bool AddDomainMap(const StringPiece& flag_value, DomainLawyer* lawyer,
                  bool (*fn)(DomainLawyer* lawyer,
                             const StringPiece& to_domain,
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
      ret &= (*fn)(lawyer, name_values[0], name_values[1], message_handler);
    }
  }
  return ret;
}

}  // namespace

RewriteGflags::RewriteGflags(const char* progname, int* argc, char*** argv) {
  ParseGflags(progname, argc, argv);
}

bool RewriteGflags::SetOptions(RewriteDriverFactory* factory,
                               RewriteOptions* options) const {
  bool ret = true;
  factory->set_filename_prefix(FLAGS_filename_prefix);
  factory->set_force_caching(FLAGS_force_caching);
  // TODO(sligocki): Remove this (redundant with option setting below).
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
  if (WasExplicitlySet("css_flatten_max_bytes")) {
    options->set_css_flatten_max_bytes(FLAGS_css_flatten_max_bytes);
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
  if (WasExplicitlySet("log_background_rewrites")) {
    options->set_log_background_rewrites(FLAGS_log_background_rewrites);
  }
  if (WasExplicitlySet("log_rewrite_timing")) {
    options->set_log_rewrite_timing(FLAGS_log_rewrite_timing);
  }
  if (WasExplicitlySet("log_url_indices")) {
    options->set_log_url_indices(FLAGS_log_url_indices);
  }
  if (WasExplicitlySet("max_html_cache_time_ms")) {
    options->set_max_html_cache_time_ms(FLAGS_max_html_cache_time_ms);
  }
  if (WasExplicitlySet("metadata_input_errors_cache_ttl_ms")) {
    options->set_metadata_input_errors_cache_ttl_ms(
        FLAGS_metadata_input_errors_cache_ttl_ms);
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
  if (WasExplicitlySet("proactively_freshen_user_facing_request")) {
    options->set_proactively_freshen_user_facing_request(
        FLAGS_proactively_freshen_user_facing_request);
  }
  if (WasExplicitlySet("serve_stale_while_revalidate_threshold_sec")) {
    options->set_serve_stale_while_revalidate_threshold_sec(
        FLAGS_serve_stale_while_revalidate_threshold_sec);
  }
  if (WasExplicitlySet("serve_split_html_in_two_chunks")) {
    options->set_serve_split_html_in_two_chunks(
        FLAGS_serve_split_html_in_two_chunks);
  }
  if (WasExplicitlySet("psa_idle_flush_time_ms")) {
    options->set_idle_flush_time_ms(FLAGS_psa_idle_flush_time_ms);
  }
  if (WasExplicitlySet("psa_flush_buffer_limit_bytes")) {
    options->set_flush_buffer_limit_bytes(FLAGS_psa_flush_buffer_limit_bytes);
  }
  if (WasExplicitlySet("image_recompress_quality")) {
    options->set_image_recompress_quality(
        FLAGS_image_recompress_quality);
  }
  if (WasExplicitlySet("image_jpeg_recompress_quality")) {
    options->set_image_jpeg_recompress_quality(
        FLAGS_image_jpeg_recompress_quality);
  }
  if (WasExplicitlySet("image_jpeg_recompress_quality_for_small_screens")) {
      options->set_image_jpeg_recompress_quality_for_small_screens(
          FLAGS_image_jpeg_recompress_quality_for_small_screens);
  }
  if (WasExplicitlySet("image_jpeg_num_progressive_scans")) {
    options->set_image_jpeg_num_progressive_scans(
        FLAGS_image_jpeg_num_progressive_scans);
  }
  if (WasExplicitlySet("image_jpeg_num_progressive_scans_for_small_screens")) {
    options->set_image_jpeg_num_progressive_scans_for_small_screens(
        FLAGS_image_jpeg_num_progressive_scans_for_small_screens);
  }
  if (WasExplicitlySet("image_webp_recompress_quality")) {
    options->set_image_webp_recompress_quality(
        FLAGS_image_webp_recompress_quality);
  }
  if (WasExplicitlySet("image_webp_recompress_quality_for_small_screens")) {
    options->set_image_webp_recompress_quality_for_small_screens(
        FLAGS_image_webp_recompress_quality_for_small_screens);
  }
  if (WasExplicitlySet("image_webp_timeout_ms")) {
    options->set_image_webp_timeout_ms(
        FLAGS_image_webp_timeout_ms);
  }
  if (WasExplicitlySet("image_limit_optimized_percent")) {
    options->set_image_limit_optimized_percent(
        FLAGS_image_limit_optimized_percent);
  }
  if (WasExplicitlySet("image_limit_resize_area_percent")) {
    options->set_image_limit_resize_area_percent(
        FLAGS_image_limit_resize_area_percent);
  }
  if (WasExplicitlySet("image_limit_rendered_area_percent")) {
    options->set_image_limit_rendered_area_percent(
        FLAGS_image_limit_rendered_area_percent);
  }
  if (WasExplicitlySet("enable_flush_early_critical_css")) {
    options->set_enable_flush_early_critical_css(
        FLAGS_enable_flush_early_critical_css);
  }
  if (WasExplicitlySet("use_selectors_for_critical_css")) {
    options->set_use_selectors_for_critical_css(
        FLAGS_use_selectors_for_critical_css);
  }
  if (WasExplicitlySet("max_inlined_preview_images_index")) {
    options->set_max_inlined_preview_images_index(
        FLAGS_max_inlined_preview_images_index);
  }
  if (WasExplicitlySet("min_image_size_low_resolution_bytes")) {
    options->set_min_image_size_low_resolution_bytes(
        FLAGS_min_image_size_low_resolution_bytes);
  }
  if (WasExplicitlySet("max_image_size_low_resolution_bytes")) {
    options->set_max_image_size_low_resolution_bytes(
        FLAGS_max_image_size_low_resolution_bytes);
  }
  if (WasExplicitlySet("max_combined_css_bytes")) {
    options->set_max_combined_css_bytes(FLAGS_max_combined_css_bytes);
  }
  if (WasExplicitlySet("max_combined_js_bytes")) {
    options->set_max_combined_js_bytes(FLAGS_max_combined_js_bytes);
  }
  if (WasExplicitlySet("finder_properties_cache_expiration_time_ms")) {
    options->set_finder_properties_cache_expiration_time_ms(
        FLAGS_finder_properties_cache_expiration_time_ms);
  }
  if (WasExplicitlySet("finder_properties_cache_refresh_time_ms")) {
    options->set_finder_properties_cache_refresh_time_ms(
        FLAGS_finder_properties_cache_refresh_time_ms);
  }
  if (WasExplicitlySet("metadata_cache_staleness_threshold_ms")) {
    options->set_metadata_cache_staleness_threshold_ms(
        FLAGS_metadata_cache_staleness_threshold_ms);
  }
  if (WasExplicitlySet("lazyload_images_after_onload")) {
    options->set_lazyload_images_after_onload(
        FLAGS_lazyload_images_after_onload);
  }
  if (WasExplicitlySet("use_blank_image_for_inline_preview")) {
    options->set_use_blank_image_for_inline_preview(
        FLAGS_use_blank_image_for_inline_preview);
  }
  if (WasExplicitlySet("use_fallback_property_cache_values")) {
    options->set_use_fallback_property_cache_values(
        FLAGS_use_fallback_property_cache_values);
  }
  if (WasExplicitlySet("await_pcache_lookup")) {
    options->set_await_pcache_lookup(FLAGS_await_pcache_lookup);
  }
  if (WasExplicitlySet("lazyload_images_blank_url")) {
    options->set_lazyload_images_blank_url(
        FLAGS_lazyload_images_blank_url);
  }
  if (WasExplicitlySet("pre_connect_url")) {
    options->set_pre_connect_url(FLAGS_pre_connect_url);
  }
  if (WasExplicitlySet("inline_only_critical_images")) {
    options->set_inline_only_critical_images(
        FLAGS_inline_only_critical_images);
  }
  // Default value for this flag in rewrite_options.cc differs from the flag
  // setting here, so set it unconditionally (they unintentionally mismatched
  // before, enabling the critical images beacon here which did nothing except
  // turn on checks we wanted switched off).
  options->set_critical_images_beacon_enabled(
      FLAGS_critical_images_beacon_enabled);
  if (WasExplicitlySet(
          "test_only_prioritize_critical_css_dont_apply_original_css")) {
    options->set_test_only_prioritize_critical_css_dont_apply_original_css(
        FLAGS_test_only_prioritize_critical_css_dont_apply_original_css);
  }
  if (WasExplicitlySet("implicit_cache_ttl_ms")) {
    options->set_implicit_cache_ttl_ms(FLAGS_implicit_cache_ttl_ms);
  }
  if (WasExplicitlySet("min_cache_ttl_ms")) {
    options->set_min_cache_ttl_ms(FLAGS_min_cache_ttl_ms);
  }
  if (WasExplicitlySet("max_prefetch_js_elements")) {
    options->set_max_prefetch_js_elements(FLAGS_max_prefetch_js_elements);
  }
  if (WasExplicitlySet("enable_defer_js_experimental")) {
    options->set_enable_defer_js_experimental(
        FLAGS_enable_defer_js_experimental);
  }
  if (WasExplicitlySet("disable_rewrite_on_no_transform")) {
    options->set_disable_rewrite_on_no_transform(
        FLAGS_disable_rewrite_on_no_transform);
  }
  if (WasExplicitlySet("disable_background_fetches_for_bots")) {
    options->set_disable_background_fetches_for_bots(
        FLAGS_disable_background_fetches_for_bots);
  }
  if (WasExplicitlySet("flush_more_resources_early_if_time_permits")) {
    options->set_flush_more_resources_early_if_time_permits(
        FLAGS_flush_more_resources_early_if_time_permits);
  }
  // TODO(pulkitg): Remove this flag when this feature gets stabilized.
  if (WasExplicitlySet("flush_more_resources_in_ie_and_firefox")) {
    options->set_flush_more_resources_in_ie_and_firefox(
        FLAGS_flush_more_resources_in_ie_and_firefox);
  }
  if (WasExplicitlySet("lazyload_highres_images")) {
    options->set_lazyload_highres_images(FLAGS_lazyload_highres_images);
  }
  if (WasExplicitlySet("image_preserve_urls")) {
    options->set_image_preserve_urls(FLAGS_image_preserve_urls);
  }
  if (WasExplicitlySet("css_preserve_urls")) {
    options->set_css_preserve_urls(FLAGS_css_preserve_urls);
  }
  if (WasExplicitlySet("js_preserve_urls")) {
    options->set_js_preserve_urls(FLAGS_js_preserve_urls);
  }
  if (WasExplicitlySet("rewrite_deadline_per_flush_ms")) {
    options->set_rewrite_deadline_ms(FLAGS_rewrite_deadline_per_flush_ms);
  }
  if (WasExplicitlySet("experiment_cookie_duration_ms")) {
    options->set_experiment_cookie_duration_ms(
        FLAGS_experiment_cookie_duration_ms);
  }
  if (WasExplicitlySet("avoid_renaming_introspective_javascript")) {
    options->set_avoid_renaming_introspective_javascript(
        FLAGS_avoid_renaming_introspective_javascript);
  }
  if (WasExplicitlySet("blocking_rewrite_key")) {
    options->set_blocking_rewrite_key(FLAGS_blocking_rewrite_key);
  }
  if (WasExplicitlySet("distributed_rewrite_key")) {
    options->set_distributed_rewrite_key(FLAGS_distributed_rewrite_key);
  }
  if (WasExplicitlySet("distributed_rewrite_servers")) {
    options->set_distributed_rewrite_servers(FLAGS_distributed_rewrite_servers);
  }
  if (WasExplicitlySet("distributed_rewrite_timeout_ms")) {
    options->set_distributed_rewrite_timeout_ms(
        FLAGS_distributed_rewrite_timeout_ms);
  }
  if (WasExplicitlySet("distribute_fetches")) {
    options->set_distribute_fetches(FLAGS_distribute_fetches);
  }
  if (WasExplicitlySet("pagespeed_version")) {
    options->set_x_header_value(FLAGS_pagespeed_version);
  }
  if (WasExplicitlySet("enable_blink_debug_dashboard")) {
    options->set_enable_blink_debug_dashboard(
        FLAGS_enable_blink_debug_dashboard);
  }
  if (WasExplicitlySet("report_unload_time")) {
    options->set_report_unload_time(FLAGS_report_unload_time);
  }
  if (WasExplicitlySet("blink_html_change_detection_time_ms")) {
    options->set_blink_html_change_detection_time_ms(
        FLAGS_blink_html_change_detection_time_ms);
  }
  if (WasExplicitlySet("enable_blink_html_change_detection")) {
    options->set_enable_blink_html_change_detection(
        FLAGS_enable_blink_html_change_detection);
  }
  if (WasExplicitlySet("enable_blink_html_change_detection_logging")) {
    options->set_enable_blink_html_change_detection_logging(
        FLAGS_enable_blink_html_change_detection_logging);
  }
  if (WasExplicitlySet("use_smart_diff_in_blink")) {
    options->set_use_smart_diff_in_blink(FLAGS_use_smart_diff_in_blink);
  }
  if (WasExplicitlySet("max_image_bytes_for_webp_in_css")) {
    options->set_max_image_bytes_for_webp_in_css(
        FLAGS_max_image_bytes_for_webp_in_css);
  }
  if (WasExplicitlySet("persist_blink_blacklist")) {
    options->set_persist_blink_blacklist(
        FLAGS_persist_blink_blacklist);
  }
  if (WasExplicitlySet("enable_prioritizing_scripts")) {
    options->set_enable_prioritizing_scripts(
        FLAGS_enable_prioritizing_scripts);
  }
  if (WasExplicitlySet("override_ie_document_mode")) {
    options->set_override_ie_document_mode(FLAGS_override_ie_document_mode);
  }
  if (WasExplicitlySet("max_html_parse_bytes")) {
    options->set_max_html_parse_bytes(FLAGS_max_html_parse_bytes);
  }
  if (WasExplicitlySet("enable_aggressive_rewriters_for_mobile")) {
    options->set_enable_aggressive_rewriters_for_mobile(
        FLAGS_enable_aggressive_rewriters_for_mobile);
  }
  if (WasExplicitlySet("lazyload_disabled_classes")) {
    GoogleString lazyload_disabled_classes_string(
        FLAGS_lazyload_disabled_classes);
    LowerString(&lazyload_disabled_classes_string);
    StringPieceVector lazyload_disabled_classes;
    SplitStringPieceToVector(lazyload_disabled_classes_string, ",",
                             &lazyload_disabled_classes, true);
    for (int i = 0, n = lazyload_disabled_classes.size(); i < n; ++i) {
      options->DisableLazyloadForClassName(lazyload_disabled_classes[i]);
    }
  }
  if (WasExplicitlySet("max_rewrite_info_log_size")) {
    options->set_max_rewrite_info_log_size(FLAGS_max_rewrite_info_log_size);
  }
  if (WasExplicitlySet("property_cache_http_status_stability_threshold")) {
    options->set_property_cache_http_status_stability_threshold(
        FLAGS_property_cache_http_status_stability_threshold);
  }

  // TODO(bolian): Remove ajax_rewriting_enabled once everyone moves to
  // in_place_rewriting_enabled.
  if (WasExplicitlySet("ajax_rewriting_enabled")) {
    options->set_in_place_rewriting_enabled(FLAGS_ajax_rewriting_enabled);
  }
  if (WasExplicitlySet("in_place_rewriting_enabled")) {
    options->set_in_place_rewriting_enabled(FLAGS_in_place_rewriting_enabled);
  }

  if (WasExplicitlySet("oblivious_pagespeed_urls")) {
    options->set_oblivious_pagespeed_urls(FLAGS_oblivious_pagespeed_urls);
  }

  if (WasExplicitlySet("rewrite_uncacheable_resources")) {
    options->set_rewrite_uncacheable_resources(
        FLAGS_rewrite_uncacheable_resources);
  }

  if (WasExplicitlySet("in_place_wait_for_optimized")) {
    options->set_in_place_wait_for_optimized(FLAGS_in_place_wait_for_optimized);
  }

  if (WasExplicitlySet("in_place_rewrite_deadline_ms")) {
    options->set_in_place_rewrite_deadline_ms(
        FLAGS_in_place_rewrite_deadline_ms);
  }

  if (WasExplicitlySet("in_place_preemptive_rewrite_css_images")) {
    options->set_in_place_preemptive_rewrite_css_images(
        FLAGS_in_place_preemptive_rewrite_css_images);
  }

  if (WasExplicitlySet("in_place_preemptive_rewrite_images")) {
    options->set_in_place_preemptive_rewrite_images(
        FLAGS_in_place_preemptive_rewrite_images);
  }
  if (WasExplicitlySet("in_place_preemptive_rewrite_css")) {
    options->set_in_place_preemptive_rewrite_css(
        FLAGS_in_place_preemptive_rewrite_css);
  }
  if (WasExplicitlySet("in_place_preemptive_rewrite_javascript")) {
    options->set_in_place_preemptive_rewrite_javascript(
        FLAGS_in_place_preemptive_rewrite_javascript);
  }
  if (WasExplicitlySet("private_not_vary_for_ie")) {
    options->set_private_not_vary_for_ie(FLAGS_private_not_vary_for_ie);
  }
  if (WasExplicitlySet("serve_ghost_click_buster_with_split_html")) {
    options->set_serve_ghost_click_buster_with_split_html(
        FLAGS_serve_ghost_click_buster_with_split_html);
  }
  if (WasExplicitlySet("serve_xhr_access_control_headers")) {
    options->set_serve_xhr_access_control_headers(
        FLAGS_serve_xhr_access_control_headers);
  }
  if (WasExplicitlySet("access_control_allow_origins")) {
    options->set_access_control_allow_origins(
        FLAGS_access_control_allow_origins);
  }
  if (WasExplicitlySet("hide_referer_using_meta")) {
    options->set_hide_referer_using_meta(
        FLAGS_hide_referer_using_meta);
  }
  if (WasExplicitlySet("max_low_res_image_size_bytes")) {
    options->set_max_low_res_image_size_bytes(
        FLAGS_max_low_res_image_size_bytes);
  }
  if (WasExplicitlySet("max_low_res_to_full_res_image_size_percentage")) {
    options->set_max_low_res_to_full_res_image_size_percentage(
        FLAGS_max_low_res_to_full_res_image_size_percentage);
  }

  MessageHandler* handler = factory->message_handler();

  StringPieceVector domains;
  SplitStringPieceToVector(FLAGS_domains, ",", &domains, true);
  if (!domains.empty()) {
    DomainLawyer* lawyer = options->WriteableDomainLawyer();
    for (int i = 0, n = domains.size(); i < n; ++i) {
      if (!lawyer->AddDomain(domains[i], handler)) {
        LOG(ERROR) << "Invalid domain: " << domains[i];
        ret = false;
      }
    }
  }

  if (WasExplicitlySet("rewrite_domain_map")) {
    ret &= AddDomainMap(FLAGS_rewrite_domain_map,
                        options->WriteableDomainLawyer(),
                        DomainMapRewriteDomain, handler);
  }

  if (WasExplicitlySet("shard_domain_map")) {
    ret &= AddDomainMap(FLAGS_shard_domain_map,
                        options->WriteableDomainLawyer(),
                        DomainAddShard, handler);
  }

  if (WasExplicitlySet("origin_domain_map")) {
    ret &= AddDomainMap(FLAGS_origin_domain_map,
                        options->WriteableDomainLawyer(),
                        DomainMapOriginDomain, handler);
  }
  if (WasExplicitlySet("enable_extended_instrumentation")) {
    options->set_enable_extended_instrumentation(
        FLAGS_enable_extended_instrumentation);
  }
  if (WasExplicitlySet("use_experimental_js_minifier")) {
    options->set_use_experimental_js_minifier(
        FLAGS_use_experimental_js_minifier);
  }
  if (WasExplicitlySet("enable_cache_purge")) {
    options->set_enable_cache_purge(FLAGS_enable_cache_purge);
  }
  if (WasExplicitlySet("proactive_resource_freshening")) {
    options->set_proactive_resource_freshening(
        FLAGS_proactive_resource_freshening);
  }

  if (WasExplicitlySet("support_noscript_enabled")) {
    options->set_support_noscript_enabled(FLAGS_support_noscript_enabled);
  }
  if (WasExplicitlySet("known_libraries")) {
    StringPieceVector library_specs;
    SplitStringPieceToVector(FLAGS_known_libraries, " ", &library_specs, true);
    int i = 0;
    for (int max = library_specs.size() - 2; i < max; i += 3) {
      int64 bytes;
      if (!StringToInt64(library_specs[i], &bytes)) {
        LOG(ERROR) << "Invalid library size in bytes; skipping: " <<
            library_specs[i];
        continue;
      }
      const StringPiece& md5 = library_specs[i + 1];
      const StringPiece& url = library_specs[i + 2];
      if (!options->RegisterLibrary(bytes, md5, url)) {
        LOG(ERROR) << "Invalid library md5 or url; skipping: " <<
            md5 << " " << url;
      }
      LOG(INFO) << "Registering library " << bytes << " " <<
          md5 << " " << url;
    }
    for (int max = library_specs.size(); i < max; ++i) {
      LOG(ERROR) << "Unused library flag " << library_specs[i];
    }
  }
  if (WasExplicitlySet("experiment_specs")) {
    options->set_running_experiment(true);
    StringPieceVector experiment_specs;
    SplitStringPieceToVector(FLAGS_experiment_specs, "+",
                             &experiment_specs, true);
    for (int i = 0, n = experiment_specs.size(); i < n; ++i) {
      if (!options->AddExperimentSpec(experiment_specs[i], handler)) {
        LOG(ERROR) << "Invalid experiment specification: "
                   << experiment_specs[i];
        ret = false;
      }
    }
  }
  if (WasExplicitlySet("distributable_filters")) {
    options->DistributeFiltersByCommaSeparatedList(FLAGS_distributable_filters,
                                                   handler);
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

bool RewriteGflags::WasExplicitlySet(const char* name) {
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
