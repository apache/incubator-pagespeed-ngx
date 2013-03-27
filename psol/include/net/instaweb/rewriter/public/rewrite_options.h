/*
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_

#include <cstddef>                      // for size_t
#include <map>
#include <set>
#include <utility>                      // for pair
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "third_party/instaweb/util/fast_wildcard_group.h"
#include "third_party/instaweb/util/wildcard.h"

namespace net_instaweb {

class GoogleUrl;
class Hasher;
class MessageHandler;

// Defines a set of customizations that can be applied to any Rewrite.  There
// are multiple categories of customizations:
//   - filter sets (controllable individually or by level)
//   - options (arbitrarily typed variables)
//   - domain customization (see DomainLawyer class).
//   - FileLoadPolicy (enables reading resources as files from the file system)
// RewriteOptions can be specified in several ways, forming a hierarchy:
//   - Globally for a process
//   - Customized per server (e.g. Apache VirtualHost)
//   - Customized at Directory level (e.g. Apache <Directory> or .htaccess)
//   - Tuned at the request-level (e.g. via request-headers or query-params).
// The hierarchy is implemented via Merging.
//
// The options are themselves a complex system.  Many Option objects are
// instantiated for each RewriteOptions instance.  RewriteOptions can be
// constructed and destroyed multiple times per request so to reduce
// this cost, the static aspects of Options are factored out into
// Properties, which are intialized once per process via
// RewriteOptions::Initialize.  Subclasses may also add new Properties
// and so property-list-merging takes place at Initialization time.
class RewriteOptions {
 public:
  // If you add or remove anything from this list, you need to update the
  // kFilterVectorStaticInitializer array in rewrite_options.cc and the
  // LookupOptionEnumTest method in rewrite_options_test.cc.  If you add
  // image-related options or css-related options, you must add them
  // to the kRelatedFilters and kRelatedOptions lists in css_filter.cc
  // and image_rewrite_filter.cc.
  enum Filter {
    kAddBaseTag,  // Update kFirstFilter if you add something before this.
    kAddHead,
    kAddInstrumentation,
    kCacheHtml,
    kCanonicalizeJavascriptLibraries,
    kCollapseWhitespace,
    kCollectFlushEarlyContentFilter,
    kCombineCss,
    kCombineHeads,
    kCombineJavascript,
    kComputeCriticalCss,
    kComputeVisibleText,
    kConvertGifToPng,
    kConvertJpegToProgressive,
    kConvertJpegToWebp,
    kConvertMetaTags,
    kConvertPngToJpeg,
    kConvertToWebpLossless,
    kDebug,
    kDecodeRewrittenUrls,
    kDeferIframe,
    kDeferJavascript,
    kDelayImages,
    kDetectReflowWithDeferJavascript,
    kDeterministicJs,
    kDisableJavascript,
    kDivStructure,
    kElideAttributes,
    kExperimentSpdy,  // Temporary and will be removed soon.
    kExplicitCloseTags,
    kExtendCacheCss,
    kExtendCacheImages,
    kExtendCachePdfs,
    kExtendCacheScripts,
    kFallbackRewriteCssUrls,
    kFlattenCssImports,
    kFlushSubresources,
    kHandleNoscriptRedirect,
    kHtmlWriterFilter,
    kInlineCss,
    kInlineImages,
    kInlineImportToLink,
    kInlineJavascript,
    kInPlaceOptimizeForBrowser,
    kInsertDnsPrefetch,
    kInsertGA,
    kInsertImageDimensions,
    kJpegSubsampling,
    kLazyloadImages,
    kLeftTrimUrls,
    kLocalStorageCache,
    kMakeGoogleAnalyticsAsync,
    kMoveCssAboveScripts,
    kMoveCssToHead,
    kOutlineCss,
    kOutlineJavascript,
    kPedantic,
    kPrioritizeCriticalCss,
    kPrioritizeVisibleContent,
    kProcessBlinkInBackground,
    kRecompressJpeg,
    kRecompressPng,
    kRecompressWebp,
    kRemoveComments,
    kRemoveQuotes,
    kResizeImages,
    kResizeMobileImages,
    kRewriteCss,
    kRewriteDomains,
    kRewriteJavascript,
    kRewriteStyleAttributes,
    kRewriteStyleAttributesWithUrl,
    kServeNonCacheableNonCritical,
    kSplitHtml,
    kSpriteImages,
    kSquashImagesForMobileScreen,
    kStripImageColorProfile,
    kStripImageMetaData,
    kStripNonCacheable,
    kStripScripts,
    kEndOfFilters
  };

  enum EnabledEnum {
    // Don't optimize HTML. Do serve .pagespeed. Can be overridden via query
    // param.
    kEnabledOff,
    // Pagespeed runs normally.  Can be overridden via query param.
    kEnabledOn,
    // Completely passive. Do not serve .pagespeed. Return from handlers
    // immediately. Cannot be overridden via query param.
    kEnabledUnplugged,
  };

  // Any new Option added, should have a corresponding enum here and this should
  // be passed in when Add*Property is called in AddProperties().
  //
  // TODO(satyanarayana): Deprecate kImageRetainColorProfile,
  // kImageRetainExifData and kImageRetainColorSampling as they are now
  // converted to filters.
  enum OptionEnum {
    kAddOptionsToUrls,
    kAllowLoggingUrlsInLogRecord,
    kAlwaysRewriteCss,
    kAnalyticsID,
    kAvoidRenamingIntrospectiveJavascript,
    kBeaconUrl,
    kBlinkDesktopUserAgent,
    kBlinkMaxHtmlSizeRewritable,
    kBlinkNonCacheablesForAllFamilies,
    kCacheInvalidationTimestamp,
    kCacheSmallImagesUnrewritten,
    kClientDomainRewrite,
    kCombineAcrossPaths,
    kCriticalImagesBeaconEnabled,
    kCriticalLineConfig,
    kCssFlattenMaxBytes,
    kCssImageInlineMaxBytes,
    kCssInlineMaxBytes,
    kCssOutlineMinBytes,
    kCssPreserveURLs,
    kDefaultCacheHtml,
    kDistributedRewriteServers,
    kDistributedRewriteTimeoutMs,
    kDomainRewriteHyperlinks,
    kDomainShardCount,
    kEnableAggressiveRewritersForMobile,
    kEnableBlinkCriticalLine,
    kEnableBlinkDashboard,
    kEnableBlinkForMobileDevices,
    kEnableBlinkHtmlChangeDetection,
    kEnableBlinkHtmlChangeDetectionLogging,
    kEnableDeferJsExperimental,
    kEnableFlushSubresourcesExperimental,
    kEnableInlinePreviewImagesExperimental,
    kEnableLazyLoadHighResImages,
    kEnableLazyloadInBlink,
    kEnablePrioritizingScripts,
    kEnabled,
    kFinderPropertiesCacheExpirationTimeMs,
    kFinderPropertiesCacheRefreshTimeMs,
    kFlushBufferLimitBytes,
    kFlushHtml,
    kFlushMoreResourcesEarlyIfTimePermits,
    kForbidAllDisabledFilters,
    kFuriousCookieDurationMs,
    kFuriousSlot,
    kIdleFlushTimeMs,
    kImageInlineMaxBytes,
    kImageJpegNumProgressiveScans,
    kImageJpegRecompressionQuality,
    kImageJpegRecompressionQualityForSmallScreens,
    kImageLimitOptimizedPercent,
    kImageLimitResizeAreaPercent,
    kImageMaxRewritesAtOnce,
    kImagePreserveURLs,
    kImageRecompressionQuality,
    kImageResolutionLimitBytes,
    kImageRetainColorProfile,
    kImageRetainColorSampling,
    kImageRetainExifData,
    kImageWebpRecompressionQuality,
    kImageWebpRecompressionQualityForSmallScreens,
    kImageWebpTimeoutMs,
    kImplicitCacheTtlMs,
    kInPlaceResourceOptimization,
    kInPlaceWaitForOptimized,
    kInPlacePreemptiveRewriteCss,
    kInPlacePreemptiveRewriteCssImages,
    kInPlacePreemptiveRewriteImages,
    kInPlacePreemptiveRewriteJavascript,
    kInPlaceRewriteDeadlineMs,
    kIncreaseSpeedTracking,
    kInlineOnlyCriticalImages,
    kJsInlineMaxBytes,
    kJsOutlineMinBytes,
    kJsPreserveURLs,
    kLazyloadImagesAfterOnload,
    kLazyloadImagesBlankUrl,
    kLogRewriteTiming,
    kLowercaseHtmlNames,
    kMaxCacheableResponseContentLength,
    kMaxCombinedJsBytes,
    kMaxHtmlCacheTimeMs,
    kMaxHtmlParseBytes,
    kMaxImageBytesForWebpInCss,
    kMaxImageSizeLowResolutionBytes,
    kMaxInlinedPreviewImagesIndex,
    kMaxRewriteInfoLogSize,
    kMaxUrlSegmentSize,
    kMaxUrlSize,
    kMetadataCacheStalenessThresholdMs,
    kMinImageSizeLowResolutionBytes,
    kMinResourceCacheTimeToRewriteMs,
    kModifyCachingHeaders,
    kObliviousPagespeedUrls,
    kOverrideBlinkCacheTimeMs,
    kOverrideCachingTtlMs,
    kOverrideIeDocumentMode,
    kPassthroughBlinkForInvalidResponseCode,
    kProgressiveJpegMinBytes,
    kPropagateBlinkCacheDeletes,
    kRejectBlacklisted,
    kRejectBlacklistedStatusCode,
    kReportUnloadTime,
    kRespectVary,
    kRespectXForwardedProto,
    kRewriteDeadlineMs,
    kRewriteLevel,
    kRewriteUncacheableResources,
    kRunningFurious,
    kServeStaleIfFetchError,
    kSupportNoScriptEnabled,
    kUseSmartDiffInBlink,
    kXModPagespeedHeaderValue,
    kXPsaBlockingRewrite,

    // Options that require special handling, e.g. non-scalar values
    kAllow,
    kDisableFilters,
    kDisallow,
    kDistributableFilters,  // For experimentation, may be removed later.
    kDomain,
    kEnableFilters,
    kExperimentVariable,
    kExperimentSpec,
    kForbidFilters,
    kRetainComment,

    // 2-argument ones:
    kCustomFetchHeader,
    kLoadFromFile,
    kLoadFromFileMatch,
    kLoadFromFileRule,
    kLoadFromFileRuleMatch,
    kMapOriginDomain,
    kMapRewriteDomain,
    kMapProxyDomain,
    kShardDomain,

    // 3-argument ones:
    kUrlValuedAttribute,
    kLibrary,

    // apache/ or system/ specific:
    kCacheFlushFilename,
    kCacheFlushPollIntervalSec,
    kCollectRefererStatistics,
    kExperimentalFetchFromModSpdy,
    kFetchHttps,
    kFetcherProxy,
    kFetcherTimeOutMs,
    kFileCacheCleanInodeLimit,
    kFileCacheCleanIntervalMs,
    kFileCacheCleanSizeKb,
    kFileCachePath,
    kHashRefererStatistics,
    kLruCacheByteLimit,
    kLruCacheKbPerProcess,
    kMemcachedServers,
    kMemcachedThreads,
    kMemcachedTimeoutUs,
    kMessageBufferSize,
    kRateLimitBackgroundFetches,
    kRefererStatisticsOutputLevel,
    kSlurpDirectory,
    kSlurpFlushLimit,
    kSlurpReadOnly,
    kStatisticsEnabled,
    kStatisticsLoggingEnabled,
    kStatisticsLoggingFile,
    kStatisticsLoggingIntervalMs,
    kStatisticsLoggingChartsCSS,
    kStatisticsLoggingChartsJS,
    kTestProxy,
    kTestProxySlurp,
    kUseSharedMemLocking,

    // This is used as a marker for unknown options, as well as to denote
    // how many options the PSOL library itself knows about.
    kEndOfOptions
  };

  struct BeaconUrl {
    GoogleString http;
    GoogleString https;
  };

  struct NameValue {
    NameValue(const StringPiece& name_in, const StringPiece& value_in) {
      name_in.CopyToString(&name);
      value_in.CopyToString(&value);
    }
    GoogleString name;
    GoogleString value;
  };

  // This version index serves as global signature key.  Much of the
  // data emitted in signatures is based on the option ordering, which
  // can change as we add new options.  So every time there is a
  // binary-incompatible change to the option ordering, we bump this
  // version.
  //
  // Note: we now use a two-letter code for identifying enabled filters, so
  // there is no need bump the option version when changing the filter enum.
  //
  // Updating this value will have the indirect effect of flushing the metadata
  // cache.
  //
  // This version number should be incremented if any default-values
  // are changed, either in an Add*Property() call or via
  // options->set_default.
  static const int kOptionsVersion = 13;

  static const char kCacheExtenderId[];
  static const char kCollectFlushEarlyContentFilterId[];

  // Determines the scope at which an option is evaluated.  In Apache,
  // for example, kDirectoryScope indicates it can be changed via .htaccess
  // files, which is the only way that sites using shared hosting can change
  // settings.
  enum OptionScope {
    kDirectoryScope,  // customized at directory level (.htaccess, <Directory>)
    kServerScope,     // customized at server level (e.g. VirtualHost)
    kProcessScope,    // customized at process level only (command-line flags)
  };

  static const char kCssCombinerId[];
  static const char kCssFilterId[];
  static const char kCssImportFlattenerId[];
  static const char kCssInlineId[];
  static const char kImageCombineId[];
  static const char kImageCompressionId[];
  static const char kInPlaceRewriteId[];
  static const char kJavascriptCombinerId[];
  static const char kJavascriptInlineId[];
  static const char kJavascriptMinId[];
  static const char kLocalStorageCacheId[];
  static const char kPrioritizeCriticalCssId[];

  static const char kPanelCommentPrefix[];

  // Return the appropriate human-readable filter name for the given filter,
  // e.g. "CombineCss".
  static const char* FilterName(Filter filter);

  // Returns a two-letter id code for this filter, used for for encoding
  // URLs.
  static const char* FilterId(Filter filter);

  // Used for enumerating over all entries in the Filter enum.
  static const Filter kFirstFilter = kAddBaseTag;

  // Convenience name for a set of rewrite filters.
  typedef std::set<Filter> FilterSet;

  // Lookup the given name to see if it's a filter name or one of the special
  // names like "core" or "rewrite_images", and if so add the corresponding
  // filter(s) to the given set. If the given name doesn't match -and- if
  // handler is not NULL, logs a warning message to handler. Returns true if
  // the name matched and the set was updated, false otherwise.
  static bool AddByNameToFilterSet(const StringPiece& option, FilterSet* set,
                                   MessageHandler* handler);

  // Convenience name for (name,value) pairs of options (typically filter
  // parameters), as well as sets of those pairs.
  typedef std::pair<StringPiece, StringPiece> OptionStringPair;
  typedef std::set<OptionStringPair> OptionSet;

  // The base class for a Property.  This class contains fields of
  // Properties that are independent of type.
  class PropertyBase {
   public:
    PropertyBase(const char* id, OptionEnum option_enum)
        : id_(id),
          help_text_(NULL),
          option_enum_(option_enum),
          scope_(kDirectoryScope),
          do_not_use_for_signature_computation_(false),
          index_(-1) {
    }
    virtual ~PropertyBase();

    // Connect the specified RewriteOption to this property.
    // set_index() must previously have been called on this.
    virtual void InitializeOption(RewriteOptions* options) const = 0;

    void set_do_not_use_for_signature_computation(bool x) {
      do_not_use_for_signature_computation_ = x;
    }
    bool is_used_for_signature_computation() const {
      return !do_not_use_for_signature_computation_;
    }

    void set_scope(OptionScope x) { scope_ = x; }
    OptionScope scope() const { return scope_; }

    void set_help_text(const char* x) { help_text_ = x; }
    const char* help_text() const { return help_text_; }

    void set_index(int index) { index_ = index; }
    const char* id() const { return id_; }
    OptionEnum option_enum() const { return option_enum_; }
    int index() const { return index_; }

   private:
    const char* id_;
    const char* help_text_;
    OptionEnum option_enum_;  // To know where this is in all_options_.
    OptionScope scope_;
    bool do_not_use_for_signature_computation_;  // Default is false.
    int index_;

    DISALLOW_COPY_AND_ASSIGN(PropertyBase);
  };

  typedef std::vector<PropertyBase*> PropertyVector;

  // Base class for Option -- the instantiation of a Property that
  // occurs in each RewriteOptions instance.
  class OptionBase {
   public:
    OptionBase() {}
    virtual ~OptionBase();

    // TODO(jmarantz): Change the 'value_string' formal to a StringPiece.
    virtual bool SetFromString(const GoogleString& value_string) = 0;
    virtual void Merge(const OptionBase* src) = 0;
    virtual bool was_set() const = 0;
    virtual GoogleString Signature(const Hasher* hasher) const = 0;
    virtual GoogleString ToString() const = 0;
    const char* id() const { return property()->id(); }
    const char* help_text() const { return property()->help_text(); }
    OptionScope scope() const { return property()->scope(); }
    OptionEnum option_enum() const { return property()->option_enum(); }
    bool is_used_for_signature_computation() const {
      return property()->is_used_for_signature_computation();
    }
    virtual const PropertyBase* property() const = 0;
  };

  // Convenience name for a set of rewrite options.
  typedef std::vector<OptionBase*> OptionBaseVector;

  enum RewriteLevel {
    // Enable no filters. Parse HTML but do not perform any
    // transformations. This is the default value. Most users should
    // explicitly enable the kCoreFilters level by calling
    // SetRewriteLevel(kCoreFilters).
    kPassThrough,

    // Enable the core set of filters. These filters are considered
    // generally safe for most sites, though even safe filters can
    // break some sites. Most users should specify this option, and
    // then optionally add or remove specific filters based on
    // specific needs.
    kCoreFilters,

    // Enable all filters intended for core, but some of which might
    // need more testing. Good for if users are willing to test out
    // the results of the rewrite more closely.
    kTestingCoreFilters,

    // Enable all filters. This includes filters you should never turn
    // on for a real page, like StripScripts!
    kAllFilters,
  };

  // Used for return value of SetOptionFromName.
  enum OptionSettingResult {
    kOptionOk,
    kOptionNameUnknown,
    kOptionValueInvalid
  };

  static const int64 kDefaultBlinkMaxHtmlSizeRewritable;
  static const int64 kDefaultCssFlattenMaxBytes;
  static const int64 kDefaultCssImageInlineMaxBytes;
  static const int64 kDefaultCssInlineMaxBytes;
  static const int64 kDefaultCssOutlineMinBytes;
  static const int64 kDefaultImageInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultJsOutlineMinBytes;
  static const int64 kDefaultProgressiveJpegMinBytes;
  static const int64 kDefaultMaxCacheableResponseContentLength;
  static const int64 kDefaultMaxHtmlCacheTimeMs;
  static const int64 kDefaultMaxHtmlParseBytes;
  static const int64 kDefaultMaxImageBytesForWebpInCss;
  static const int64 kDefaultMetadataInputErrorsCacheTtlMs;
  static const int64 kDefaultMinResourceCacheTimeToRewriteMs;
  static const int64 kDefaultCacheInvalidationTimestamp;
  static const int64 kDefaultIdleFlushTimeMs;
  static const int64 kDefaultFlushBufferLimitBytes;
  static const int64 kDefaultImplicitCacheTtlMs;
  static const int64 kDefaultPrioritizeVisibleContentCacheTimeMs;
  static const char kDefaultBeaconUrl[];
  static const int64 kDefaultImagesRecompressQuality;
  static const int64 kDefaultImageJpegRecompressQuality;
  static const int64 kDefaultImageJpegRecompressQualityForSmallScreens;
  static const int kDefaultImageLimitOptimizedPercent;
  static const int kDefaultImageLimitResizeAreaPercent;
  static const int64 kDefaultImageResolutionLimitBytes;
  static const int kDefaultImageJpegNumProgressiveScans;
  static const int64 kDefaultImageWebpRecompressQuality;
  static const int64 kDefaultImageWebpRecompressQualityForSmallScreens;
  static const int64 kDefaultImageWebpTimeoutMs;
  static const int kDefaultDomainShardCount;
  static const int64 kDefaultBlinkHtmlChangeDetectionTimeMs;
  static const int64 kDefaultOverrideBlinkCacheTimeMs;

  // IE limits URL size overall to about 2k characters.  See
  // http://support.microsoft.com/kb/208427/EN-US
  static const int kDefaultMaxUrlSize;

  static const int kDefaultImageMaxRewritesAtOnce;

  // See http://code.google.com/p/modpagespeed/issues/detail?id=9
  // Apache evidently limits each URL path segment (between /) to
  // about 256 characters.  This is not fundamental URL limitation
  // but is Apache specific.
  static const int kDefaultMaxUrlSegmentSize;

  // Default time to wait for rewrite before returning original resource.
  static const int kDefaultRewriteDeadlineMs;

  // Default time to wait for a distributed rewrite to return.
  static const int64 kDefaultDistributedTimeoutMs;

  // Default number of first N images for which low res image is generated by
  // DelayImagesFilter.
  static const int kDefaultMaxInlinedPreviewImagesIndex;
  // Default minimum image size above which low res image is generated by
  // InlinePreviewImagesFilter.
  static const int64 kDefaultMinImageSizeLowResolutionBytes;
  // Default maximum image size below which low res image is generated by
  // InlinePreviewImagesFilter.
  static const int64 kDefaultMaxImageSizeLowResolutionBytes;
  // Default cache expiration value for finder properties in pcache.
  static const int64 kDefaultFinderPropertiesCacheExpirationTimeMs;
  // Default cache refresh value for finder properties in pcache.
  static const int64 kDefaultFinderPropertiesCacheRefreshTimeMs;

  // Default duration after which the furious experiment cookie will expire
  // on the user's browser.
  static const int64 kDefaultFuriousCookieDurationMs;

  // Default time in milliseconds for which a metadata cache entry may be used
  // after expiry.
  static const int64 kDefaultMetadataCacheStalenessThresholdMs;

  // Default maximum size of the combined js resource generated by JsCombiner.
  static const int64 kDefaultMaxCombinedJsBytes;

  static const int kDefaultFuriousTrafficPercent;
  // Default Custom Variable slot in which to put Furious information.
  static const int kDefaultFuriousSlot;

  static const char kDefaultBlockingRewriteKey[];

  static const char kRejectedRequestUrlKeyName[];

  static const int kDefaultPropertyCacheHttpStatusStabilityThreshold;

  static const int kDefaultMaxRewriteInfoLogSize;

  // This class is a separate subset of options for running a furious
  // experiment.
  // These options can be specified by a spec string that looks like:
  // "id=<number greater than 0>;level=<rewrite level>;enabled=
  // <comma-separated-list of filters to enable>;disabled=
  // <comma-separated-list of filters to disable>;css_inline_threshold=
  // <max size of css to inline>;image_inline_threshold=<max size of
  // image to inline>;js_inline_threshold=<max size of js to inline>.
  class FuriousSpec {
   public:
    // Creates a FuriousSpec parsed from spec.
    // If spec doesn't have an id, then id_ will be set to
    // furious::kFuriousNotSet.  These FuriousSpecs will then be rejected
    // by AddFuriousSpec().
    FuriousSpec(const StringPiece& spec, RewriteOptions* options,
                MessageHandler* handler);

    // Creates a FuriousSpec with id_=id.  All other variables
    // are initialized to 0.
    // This is primarily used for setting up the control and for cloning.
    explicit FuriousSpec(int id);

    virtual ~FuriousSpec();

    // Return a FuriousSpec with all the same information as this one.
    virtual FuriousSpec* Clone();

    bool is_valid() const { return id_ >= 0; }

    // Accessors.
    int id() const { return id_; }
    int percent() const { return percent_; }
    GoogleString ga_id() const { return ga_id_; }
    int slot() const { return ga_variable_slot_; }
    RewriteLevel rewrite_level() const { return rewrite_level_; }
    FilterSet enabled_filters() const { return enabled_filters_; }
    FilterSet disabled_filters() const { return disabled_filters_; }
    OptionSet filter_options() const { return filter_options_; }
    int64 css_inline_max_bytes() const { return css_inline_max_bytes_; }
    int64 js_inline_max_bytes() const { return js_inline_max_bytes_; }
    int64 image_inline_max_bytes() const { return image_inline_max_bytes_; }
    bool use_default() const { return use_default_; }

   protected:
    // Merges a spec into this. This follows the same semantics as
    // RewriteOptions. Specifically, filter/options list get unioned, and
    // vars get overwritten, except ID.
    void Merge(const FuriousSpec& spec);

   private:
    FRIEND_TEST(RewriteOptionsTest, FuriousMergeTest);

    // Initialize parses spec and sets the FilterSets, rewrite level,
    // inlining thresholds, and OptionSets accordingly.
    void Initialize(const StringPiece& spec, MessageHandler* handler);

    int id_;  // id for this experiment
    GoogleString ga_id_;  // Google Analytics ID for this experiment
    int ga_variable_slot_;
    int percent_;  // percentage of traffic to go through this experiment.
    RewriteLevel rewrite_level_;
    FilterSet enabled_filters_;
    FilterSet disabled_filters_;
    OptionSet filter_options_;
    int64 css_inline_max_bytes_;
    int64 js_inline_max_bytes_;
    int64 image_inline_max_bytes_;
    // Use whatever RewriteOptions' settings are without experiments
    // for this experiment.
    bool use_default_;
    DISALLOW_COPY_AND_ASSIGN(FuriousSpec);
  };

  // Represents the content type of user-defined url-valued attributes.
  struct ElementAttributeCategory {
    GoogleString element;
    GoogleString attribute;
    semantic_type::Category category;
  };

  // Identifies static properties of RewriteOptions that must be
  // initialized before the properties can be used.  Primarily for the
  // benefit of unit tests and valgrind sanity, Initialize/Terminate
  // is balance-checked.
  //
  // TODO(jmarantz): Add static properties -- currently there are none.
  class Properties {
   public:
    // Initializes a static Properties* object.  Pass the address of a static
    // member variable.  A count is kept of how many times Initialize is called.
    //
    // True will be returned if this was the first call to initialize
    // the properties object, and this can be used by implementations
    // to decide whether to initialize other static variables.
    //
    // Initialization is not thread-safe.
    static bool Initialize(Properties** properties);

    // Terminates a static Properties* object.  Pass the address of a static
    // member variable.
    //
    // True will be returned if Terminate has been called the same number
    // of times as Initialize is called, and this can be used to decide
    // whether to clean up other static variables.
    //
    // Termination is not thread-safe.
    static bool Terminate(Properties** properties_handle);

    // Returns the number of properties
    int size() const { return property_vector_.size(); }

    const PropertyBase* property(int index) const {
      return property_vector_[index];
    }
    PropertyBase* property(int index) { return property_vector_[index]; }

    // Merges the passed-in property-vector into this one, sorting the
    // merged properties.  Each property's needs its index into the
    // merged vector for initializing subclass-specific Options in
    // each constructor.  So this method mutates its input by setting
    // an index field in each property.
    void Merge(Properties* properties);

    void push_back(PropertyBase* p) { property_vector_.push_back(p); }

   private:
    // This object should not be constructed/destructed directly; it should be
    // created by calling Properties::Initialize and Properties::Terminate.
    Properties();
    ~Properties();

    // initialization_count_ acts as a reference count: it is incremented on
    // Initialize(), and decremented on Terminate().  At 0 the object is
    // deleted.
    int initialization_count_;

    // owns_properties_ is set to true if the PropertyBase* in the vector should
    // be deleted when Terminate is called bringing initialization_count_ to 0.
    //   RewriteOptions::properties_.owns_properties_ is true.
    //   RewriteOptions::all_properties_.owns_properties_ is false.
    bool owns_properties_;
    PropertyVector property_vector_;
  };

  // Maps a filter's enum (kAddHead) to its id ("ah") and name ("Add Head").
  struct FilterEnumToIdAndNameEntry {
    RewriteOptions::Filter filter_enum;
    const char* filter_id;
    const char* filter_name;
  };

  static bool ParseRewriteLevel(const StringPiece& in, RewriteLevel* out);

  // Parse a beacon url, or a pair of beacon urls (http https) separated by a
  // space.  If only an http url is given, the https url is derived from it
  // by simply substituting the protocol.
  static bool ParseBeaconUrl(const StringPiece& in, BeaconUrl* out);

  // Checks if either of the optimizing rewrite options are ON and it includes
  // kRecompressJPeg, kRecompressPng, kRecompressWebp, kConvertGifToPng,
  // kConvertJpegToWebp, kConvertPngToJpeg, and kConvertToWebpLossless.
  bool ImageOptimizationEnabled() const;

  RewriteOptions();
  virtual ~RewriteOptions();

  // Static initialization of members.  Calls to Initialize and
  // Terminate must be matched.  Returns 'true' for the first
  // Initialize call and the last Terminate call.
  static bool Initialize();
  static bool Terminate();

  // Initializes the Options objects in a RewriteOptions instance
  // based on the supplied Properties vector.  Note that subclasses
  // can statically define additional properties, in which case they
  // should call this method from their constructor.
  void InitializeOptions(const Properties* properties);

  bool modified() const { return modified_; }

  // Sets the default rewrite level for this RewriteOptions object only.
  // Note that the defaults for other RewriteOptions objects are unaffected.
  //
  // TODO(jmarantz): Get rid of this method.  The semantics it requires are
  // costly to implement and don't add much value.
  void SetDefaultRewriteLevel(RewriteLevel level) {
    // Do not set the modified bit -- we are only changing the default.
    level_.set_default(level);
  }
  void SetRewriteLevel(RewriteLevel level) {
    set_option(level, &level_);
  }

  // Specify a header to insert when fetching subresources.
  void AddCustomFetchHeader(const StringPiece& name, const StringPiece& value);

  const NameValue* custom_fetch_header(int i) const {
    return custom_fetch_headers_[i];
  }

  int num_custom_fetch_headers() const {
    return custom_fetch_headers_.size();
  }

  // Returns the spec with the id_ that matches id.  Returns NULL if no
  // spec matches.
  FuriousSpec* GetFuriousSpec(int id) const;

  // Returns false if id is negative, or if the id is reserved
  // for NoExperiment or NotSet, or if we already have an experiment
  // with that id.
  bool AvailableFuriousId(int id);

  // Creates a FuriousSpec from spec and adds it to the configuration.
  // Returns true if it was added successfully.
  virtual bool AddFuriousSpec(const StringPiece& spec, MessageHandler* handler);

  // Sets which side of the experiment these RewriteOptions are on.
  // Cookie-setting must be done separately.
  // furious::kFuriousNotSet indicates it hasn't been set.
  // furious::kFuriousNoExperiment indicates this request shouldn't be
  // in any experiment.
  // Then sets the rewriters to match the experiment indicated by id.
  // Returns true if succeeded in setting state.
  virtual bool SetFuriousState(int id);

  // We encode experiment information in urls as an experiment index: the first
  // ExperimentSpec is a, the next is b, and so on.  Empty string or an invalid
  // letter means kFuriousNoExperiment.
  void SetFuriousStateStr(const StringPiece& experiment_index);

  int furious_id() const { return furious_id_; }

  int furious_spec_id(int i) const {
    return furious_specs_[i]->id();
  }

  // Returns a string representation of furious_id() suitable for consumption by
  // SetFuriousStateStr(), encoding the index of the current experiment (not its
  // id).  If we're not running furious, returns the empty string.
  GoogleString GetFuriousStateStr() const;

  FuriousSpec* furious_spec(int i) const {
    return furious_specs_[i];
  }

  int num_furious_experiments() const { return furious_specs_.size(); }

  // Store that when we see <element attribute=X> we should treat X as a URL
  // pointing to a resource of the type indicated by category.  For example,
  // while by default we would treat the 'src' attribute of an a 'img' element
  // as the URL for an image and will cache-extend, inline, or otherwise
  // optimize it as appropriate, we would not do the same for the 'src'
  // atrtribute of a 'span' element (<span src=...>) because there's no "src"
  // attribute of "span" in the HTML spec.  If someone needed us to treat
  // span.src as a URL, however, they could call:
  //    AddUrlValuedAttribute("src", "span", appropriate_category)
  //
  // Makes copies of element and attribute.
  void AddUrlValuedAttribute(const StringPiece& element,
                             const StringPiece& attribute,
                             semantic_type::Category category);

  // Look up a url-valued attribute, return details via element, attribute,
  // and category.  index must be less than num_url_valued_attributes().
  void UrlValuedAttribute(int index,
                          StringPiece* element,
                          StringPiece* attribute,
                          semantic_type::Category* category) const;

  int num_url_valued_attributes() const {
    if (url_valued_attributes_ == NULL) {
      return 0;
    } else {
      return url_valued_attributes_->size();
    }
  }

  // Store size, md5 hash and canonical url for library recognition.
  bool RegisterLibrary(
      uint64 bytes, StringPiece md5_hash, StringPiece canonical_url) {
    return javascript_library_identification_.RegisterLibrary(
        bytes, md5_hash, canonical_url);
  }

  // Return the javascript_library_identification_ object that applies to
  // the current configuration (NULL if identification is disabled).
  const JavascriptLibraryIdentification* javascript_library_identification()
      const {
    if (Enabled(kCanonicalizeJavascriptLibraries)) {
      return &javascript_library_identification_;
    } else {
      return NULL;
    }
  }

  RewriteLevel level() const { return level_.value(); }

  // Enables filters specified without a prefix or with a prefix of '+' and
  // disables filters specified with a prefix of '-'. Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool AdjustFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Adds a set of filters to the enabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool EnableFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Adds a set of filters to the disabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool DisableFiltersByCommaSeparatedList(const StringPiece& filters,
                                          MessageHandler* handler);

  // Adds a set of filters to the forbidden set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool ForbidFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Explicitly disable all filters which are not *currently* explicitly enabled
  //
  // Note: Do not call EnableFilter(...) for this options object after calling
  // DisableAllFilters..., because the Disable list will not be auto-updated.
  //
  // Used to deal with query param ?ModPagespeedFilter=foo
  // Which implies that all filters not listed should be disabled.
  void DisableAllFiltersNotExplicitlyEnabled();

  // Adds a set of filters to the distributable set. Returns false if any of the
  // filter names are invalid, but all the valid ones will be added anyway.
  // For experimentation, may be removed later.
  bool DistributeFiltersByCommaSeparatedList(const StringPiece& filters,
                                                   MessageHandler* handler);
  // Adds the filter to the list of distributable filters.
  // For experimentation, may be removed later.
  void DistributeFilter(Filter filter);

  // Returns true if the filter is in the list of distributable filters.
  // For experimentation, may be removed later.
  bool Distributable(Filter filter) const;

  // Adds the filter to the list of enabled filters. However, if the filter
  // is also present in either the list of disabled or forbidden filters,
  // that takes precedence and it is not enabled.
  void EnableFilter(Filter filter);
  // Guarantees that a filter would be enabled even if it is present in the list
  // of disabled filters by removing it from disabled & forbidden filter lists.
  void ForceEnableFilter(Filter filter);
  void DisableFilter(Filter filter);
  void ForbidFilter(Filter filter);
  void EnableFilters(const FilterSet& filter_set);
  void DisableFilters(const FilterSet& filter_set);
  void ForbidFilters(const FilterSet& filter_set);
  // Clear all explicitly enabled and disabled filters. Some filters may still
  // be enabled by the rewrite level and HtmlWriterFilter will be enabled.
  void ClearFilters();

  // Enables extend_cache_css, extend_cache_images, and extend_cache_scripts.
  // Does not enable extend_cache_pdfs.
  void EnableExtendCacheFilters();

  bool Enabled(Filter filter) const;
  bool Forbidden(StringPiece filter_id) const;

  // Returns the set of enabled filters that require JavaScript for execution.
  void GetEnabledFiltersRequiringScriptExecution(FilterSet* filter_set) const;

  // Disables all filters that depend on executing custom javascript.
  void DisableFiltersRequiringScriptExecution();

  // Adds pairs of (option, value) to the option set. The option names and
  // values are not checked for validity, just stored. If the string piece
  // was parsed correctly, this returns true. If there were parsing errors this
  // returns false. The set is still populated on error.
  static bool AddCommaSeparatedListToOptionSet(
      const StringPiece& options, OptionSet* set, MessageHandler* handler);

  // Set Option 'name' to 'value'. Returns whether it succeeded or the kind of
  // failure (wrong name or value), and writes the diagnostic into 'msg'.
  // This only understands simple scalar options, and not more general things
  // like filter lists, blacklists, etc.
  OptionSettingResult SetOptionFromName(
      StringPiece name, StringPiece value, GoogleString* msg);

  // Advanced option parsing, that can understand non-scalar values
  // (unlike SetOptionFromName), and which is extensible by platforms.
  // Returns whether succeeded or the kind of failure, and writes the
  // diagnostic into *msg. These are implemented in terms of the
  // corresponding ParseAndSetOptionFromEnumN methods.
  OptionSettingResult ParseAndSetOptionFromName1(
      StringPiece name, StringPiece arg,
      GoogleString* msg, MessageHandler* handler);

  OptionSettingResult ParseAndSetOptionFromName2(
      StringPiece name, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler);

  OptionSettingResult ParseAndSetOptionFromName3(
      StringPiece name, StringPiece arg1, StringPiece arg2, StringPiece arg3,
      GoogleString* msg, MessageHandler* handler);

  // See description of ParseAndSetOptionFromName1 above. This is the step
  // of that function that occurs after the name has been turned into an
  // enum.
  virtual OptionSettingResult ParseAndSetOptionFromEnum1(
      OptionEnum name, StringPiece arg,
      GoogleString* msg, MessageHandler* handler);

  virtual OptionSettingResult ParseAndSetOptionFromEnum2(
      OptionEnum name, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler);

  virtual OptionSettingResult ParseAndSetOptionFromEnum3(
      OptionEnum name, StringPiece arg1, StringPiece arg2, StringPiece arg3,
      GoogleString* msg, MessageHandler* handler);

  // Given an option specified as an enum, set its value.
  OptionSettingResult SetOptionFromEnum(OptionEnum option_enum,
                                        StringPiece value);

  // Returns the id and value of the specified option-enum in *id and *value.
  // Sets *was_set to true if this option has been altered from the default.
  //
  // If this option was not found, false is returned, and *id, *was_set, and
  // *value will be left unassigned.
  bool OptionValue(OptionEnum option_enum, const char** id,
                   bool* was_set, GoogleString* value) const;

  // Set all of the options to their values specified in the option set.
  // Returns true if all options in the set were successful, false if not.
  bool SetOptionsFromName(const OptionSet& option_set);

  // Sets Option 'name' to 'value'. Returns whether it succeeded and logs
  // any warnings to 'handler'.
  bool SetOptionFromNameAndLog(StringPiece name,
                               StringPiece value,
                               MessageHandler* handler);

  // These static methods are used by Option<T>::SetFromString to set
  // Option<T>::value_ from a string representation of it.
  static bool ParseFromString(const GoogleString& value_string, bool* value);
  static bool ParseFromString(const GoogleString& value_string,
                              EnabledEnum* value);
  static bool ParseFromString(const GoogleString& value_string, int* value) {
    return StringToInt(value_string, value);
  }
  static bool ParseFromString(const GoogleString& value_string, int64* value) {
    return StringToInt64(value_string, value);
  }
  static bool ParseFromString(const GoogleString& value_string,
                              GoogleString* value) {
    *value = value_string;
    return true;
  }
  static bool ParseFromString(const GoogleString& value_string,
                              RewriteLevel* value) {
    return ParseRewriteLevel(value_string, value);
  }
  static bool ParseFromString(const GoogleString& value_string,
                              BeaconUrl* value) {
    return ParseBeaconUrl(value_string, value);
  }

  // TODO(jmarantz): consider setting flags in the set_ methods so that
  // first's explicit settings can override default values from second.
  int64 css_outline_min_bytes() const { return css_outline_min_bytes_.value(); }
  void set_css_outline_min_bytes(int64 x) {
    set_option(x, &css_outline_min_bytes_);
  }

  GoogleString ga_id() const { return ga_id_.value(); }
  void set_ga_id(GoogleString id) {
    set_option(id, &ga_id_);
  }

  bool increase_speed_tracking() const {
    return increase_speed_tracking_.value();
  }
  void set_increase_speed_tracking(bool x) {
    set_option(x, &increase_speed_tracking_);
  }

  int64 js_outline_min_bytes() const { return js_outline_min_bytes_.value(); }
  void set_js_outline_min_bytes(int64 x) {
    set_option(x, &js_outline_min_bytes_);
  }

  int64 progressive_jpeg_min_bytes() const {
    return progressive_jpeg_min_bytes_.value();
  }
  void set_progressive_jpeg_min_bytes(int64 x) {
    set_option(x, &progressive_jpeg_min_bytes_);
  }

  int64 css_flatten_max_bytes() const { return css_flatten_max_bytes_.value(); }
  void set_css_flatten_max_bytes(int64 x) {
    set_option(x, &css_flatten_max_bytes_);
  }
  bool cache_small_images_unrewritten() const {
    return cache_small_images_unrewritten_.value();
  }
  void set_cache_small_images_unrewritten(bool x) {
    set_option(x, &cache_small_images_unrewritten_);
  }
  int64 image_resolution_limit_bytes() const {
    return image_resolution_limit_bytes_.value();
  }
  void set_image_resolution_limit_bytes(int64 x) {
    set_option(x, &image_resolution_limit_bytes_);
  }

  // Retrieve the image inlining threshold, but return 0 if it's disabled.
  int64 ImageInlineMaxBytes() const;
  void set_image_inline_max_bytes(int64 x);
  // Retrieve the css image inlining threshold, but return 0 if it's disabled.
  int64 CssImageInlineMaxBytes() const;
  void set_css_image_inline_max_bytes(int64 x) {
    set_option(x, &css_image_inline_max_bytes_);
  }
  // The larger of ImageInlineMaxBytes and CssImageInlineMaxBytes.
  int64 MaxImageInlineMaxBytes() const;
  int64 css_inline_max_bytes() const { return css_inline_max_bytes_.value(); }
  void set_css_inline_max_bytes(int64 x) {
    set_option(x, &css_inline_max_bytes_);
  }
  int64 js_inline_max_bytes() const { return js_inline_max_bytes_.value(); }
  void set_js_inline_max_bytes(int64 x) {
    set_option(x, &js_inline_max_bytes_);
  }
  int64 max_html_cache_time_ms() const {
    return max_html_cache_time_ms_.value();
  }
  void set_max_html_cache_time_ms(int64 x) {
    set_option(x, &max_html_cache_time_ms_);
  }
  int64 max_html_parse_bytes() const {
    return max_html_parse_bytes_.value();
  }
  void set_max_html_parse_bytes(int64 x) {
    set_option(x, &max_html_parse_bytes_);
  }
  int64 max_image_bytes_for_webp_in_css() const {
    return max_image_bytes_for_webp_in_css_.value();
  }
  void set_max_image_bytes_for_webp_in_css(int64 x) {
    set_option(x, &max_image_bytes_for_webp_in_css_);
  }
  int64 max_cacheable_response_content_length() const {
    return max_cacheable_response_content_length_.value();
  }
  void set_max_cacheable_response_content_length(int64 x) {
    set_option(x, &max_cacheable_response_content_length_);
  }
  int64 min_resource_cache_time_to_rewrite_ms() const {
    return min_resource_cache_time_to_rewrite_ms_.value();
  }
  void set_min_resource_cache_time_to_rewrite_ms(int64 x) {
    set_option(x, &min_resource_cache_time_to_rewrite_ms_);
  }
  bool need_to_store_experiment_data() const {
    return need_to_store_experiment_data_;
  }
  void set_need_to_store_experiment_data(bool x) {
    need_to_store_experiment_data_ = x;
  }

  int64 blocking_fetch_timeout_ms() const {
    return blocking_fetch_timeout_ms_.value();
  }
  void set_blocking_fetch_timeout_ms(int64 x) {
    set_option(x, &blocking_fetch_timeout_ms_);
  }
  bool override_ie_document_mode() const {
    return override_ie_document_mode_.value();
  }
  void set_override_ie_document_mode(bool x) {
    set_option(x, &override_ie_document_mode_);
  }

  bool is_blink_auto_blacklisted() const {
    return is_blink_auto_blacklisted_.value();
  }
  void set_is_blink_auto_blacklisted(bool x) {
    set_option(x, &is_blink_auto_blacklisted_);
  }

  // Returns false if there is an entry in url_cache_invalidation_entries_ with
  // its timestamp_ms > time_ms and url matches the url_pattern.  Else, return
  // true.
  bool IsUrlCacheValid(StringPiece url, int64 time_ms) const;

  // If timestamp_ms greater than or equal to the last timestamp in
  // url_cache_invalidation_entries_, then appends an UrlCacheInvalidationEntry
  // with 'timestamp_ms' and 'url_pattern' to url_cache_invalidation_entries_.
  // Else does nothing.
  void AddUrlCacheInvalidationEntry(StringPiece url_pattern,
                                    int64 timestamp_ms,
                                    bool is_strict);

  // Checks if url_cache_invalidation_entries_ is in increasing order of
  // timestamp.  For testing.
  bool IsUrlCacheInvalidationEntriesSorted() const;

  // Supply optional mutex for setting a global cache invalidation
  // timestamp.  Ownership of 'lock' is transfered to this.
  void set_cache_invalidation_timestamp_mutex(ThreadSystem::RWLock* lock) {
    cache_invalidation_timestamp_.set_mutex(lock);
  }

  // Cache invalidation timestamp is in milliseconds since 1970.
  int64 cache_invalidation_timestamp() const {
    ThreadSystem::ScopedReader lock(cache_invalidation_timestamp_.mutex());
    return cache_invalidation_timestamp_.value();
  }

  // Sets the cache invalidation timestamp -- in milliseconds since
  // 1970.  This function is meant to be called on a RewriteOptions*
  // immediately after instantiation.  It cannot be used to mutate the
  // value of one already in use in a RewriteDriver.
  //
  // See also UpdateCacheInvalidationTimestampMs.
  void set_cache_invalidation_timestamp(int64 timestamp_ms) {
    cache_invalidation_timestamp_.mutex()->DCheckLocked();
    DCHECK_LT(0, timestamp_ms);
    set_option(timestamp_ms, &cache_invalidation_timestamp_);
  }

  // Updates the cache invalidation timestamp of a mutexed RewriteOptions
  // instance.  Currently this only occurs in Apache global_options,
  // and is used for purging cache by touching a file in the cache directory.
  //
  // This function ignores requests to move the invalidation timestamp
  // backwards.  It returns true if the timestamp was actually changed.
  bool UpdateCacheInvalidationTimestampMs(int64 timestamp_ms,
                                          const Hasher* hasher);

  // How much inactivity of HTML input will result in PSA introducing a flush.
  // Values <= 0 disable the feature.
  int64 idle_flush_time_ms() const {
    return idle_flush_time_ms_.value();
  }
  void set_idle_flush_time_ms(int64 x) {
    set_option(x, &idle_flush_time_ms_);
  }

  // How much accumulated HTML will result in PSA introducing a flush.
  int64 flush_buffer_limit_bytes() const {
    return flush_buffer_limit_bytes_.value();
  }

  void set_flush_buffer_limit_bytes(int64 x) {
    set_option(x, &flush_buffer_limit_bytes_);
  }

  // The maximum length of a URL segment.
  // for http://a/b/c.d, this is == strlen("c.d")
  int max_url_segment_size() const { return max_url_segment_size_.value(); }
  void set_max_url_segment_size(int x) {
    set_option(x, &max_url_segment_size_);
  }

  int image_max_rewrites_at_once() const {
    return image_max_rewrites_at_once_.value();
  }
  void set_image_max_rewrites_at_once(int x) {
    set_option(x, &image_max_rewrites_at_once_);
  }

  // The maximum size of the entire URL.  If '0', this is left unlimited.
  int max_url_size() const { return max_url_size_.value(); }
  void set_max_url_size(int x) {
    set_option(x, &max_url_size_);
  }

  int rewrite_deadline_ms() const { return rewrite_deadline_ms_.value(); }
  void set_rewrite_deadline_ms(int x) {
    set_option(x, &rewrite_deadline_ms_);
  }

  bool test_instant_fetch_rewrite_deadline() const {
    return test_instant_fetch_rewrite_deadline_.value();
  }
  void set_test_instant_fetch_rewrite_deadline(bool x) {
    set_option(x, &test_instant_fetch_rewrite_deadline_);
  }

  int domain_shard_count() const { return domain_shard_count_.value(); }
  // The argument is int64 to allow it to be set from the http header or url
  // query param and int64_query_params_ only allows setting of 64 bit values.
  void set_domain_shard_count(int64 x) {
    int value = x;
    set_option(value, &domain_shard_count_);
  }

  void set_enabled(EnabledEnum x) {
    set_option(x, &enabled_);
  }
  bool enabled() const {
    return enabled_.value() == kEnabledOn;
  }
  bool unplugged() const {
    return enabled_.value() == kEnabledUnplugged;
  }

  void set_add_options_to_urls(bool x) {
    set_option(x, &add_options_to_urls_);
  }

  bool add_options_to_urls() const {
    return add_options_to_urls_.value();
  }

  void set_in_place_rewriting_enabled(bool x) {
    set_option(x, &in_place_rewriting_enabled_);
  }

  void set_oblivious_pagespeed_urls(bool x) {
    set_option(x, &oblivious_pagespeed_urls_);
  }

  bool oblivious_pagespeed_urls() const {
    return oblivious_pagespeed_urls_.value();
  }

  bool in_place_rewriting_enabled() const {
    return in_place_rewriting_enabled_.value();
  }

  void set_in_place_wait_for_optimized(bool x) {
    set_option(x, &in_place_wait_for_optimized_);
  }

  bool in_place_wait_for_optimized() const {
    return in_place_wait_for_optimized_.value();
  }

  void set_in_place_rewrite_deadline_ms(int x) {
    set_option(x, &in_place_rewrite_deadline_ms_);
  }

  int in_place_rewrite_deadline_ms() const {
    return in_place_rewrite_deadline_ms_.value();
  }

  void set_in_place_preemptive_rewrite_css(bool x) {
    set_option(x, &in_place_preemptive_rewrite_css_);
  }
  bool in_place_preemptive_rewrite_css() const {
    return in_place_preemptive_rewrite_css_.value();
  }

  void set_in_place_preemptive_rewrite_css_images(bool x) {
    set_option(x, &in_place_preemptive_rewrite_css_images_);
  }
  bool in_place_preemptive_rewrite_css_images() const {
    return in_place_preemptive_rewrite_css_images_.value();
  }

  void set_in_place_preemptive_rewrite_images(bool x) {
    set_option(x, &in_place_preemptive_rewrite_images_);
  }
  bool in_place_preemptive_rewrite_images() const {
    return in_place_preemptive_rewrite_images_.value();
  }

  void set_in_place_preemptive_rewrite_javascript(bool x) {
    set_option(x, &in_place_preemptive_rewrite_javascript_);
  }
  bool in_place_preemptive_rewrite_javascript() const {
    return in_place_preemptive_rewrite_javascript_.value();
  }

  void set_combine_across_paths(bool x) {
    set_option(x, &combine_across_paths_);
  }
  bool combine_across_paths() const { return combine_across_paths_.value(); }

  void set_log_rewrite_timing(bool x) {
    set_option(x, &log_rewrite_timing_);
  }
  bool log_rewrite_timing() const { return log_rewrite_timing_.value(); }

  void set_lowercase_html_names(bool x) {
    set_option(x, &lowercase_html_names_);
  }
  bool lowercase_html_names() const { return lowercase_html_names_.value(); }

  void set_always_rewrite_css(bool x) {
    set_option(x, &always_rewrite_css_);
  }
  bool always_rewrite_css() const { return always_rewrite_css_.value(); }

  void set_respect_vary(bool x) {
    set_option(x, &respect_vary_);
  }
  bool respect_vary() const { return respect_vary_.value(); }

  void set_respect_x_forwarded_proto(bool x) {
    set_option(x, &respect_x_forwarded_proto_);
  }
  bool respect_x_forwarded_proto() const {
    return respect_x_forwarded_proto_.value();
  }

  void set_flush_html(bool x) { set_option(x, &flush_html_); }
  bool flush_html() const { return flush_html_.value(); }

  void set_serve_stale_if_fetch_error(bool x) {
    set_option(x, &serve_stale_if_fetch_error_);
  }
  bool serve_stale_if_fetch_error() const {
    return serve_stale_if_fetch_error_.value();
  }

  void set_enable_blink_critical_line(bool x) {
    set_option(x, &enable_blink_critical_line_);
  }
  bool enable_blink_critical_line() const {
    return enable_blink_critical_line_.value();
  }

  void set_default_cache_html(bool x) { set_option(x, &default_cache_html_); }
  bool default_cache_html() const { return default_cache_html_.value(); }

  void set_modify_caching_headers(bool x) {
    set_option(x, &modify_caching_headers_);
  }
  bool modify_caching_headers() const {
    return modify_caching_headers_.value();
  }

  void set_inline_only_critical_images(bool x) {
    set_option(x, &inline_only_critical_images_);
  }
  bool inline_only_critical_images() const {
    return inline_only_critical_images_.value();
  }

  void set_critical_images_beacon_enabled(bool x) {
    set_option(x, &critical_images_beacon_enabled_);
  }
  bool critical_images_beacon_enabled() const {
    return critical_images_beacon_enabled_.value();
  }

  void set_lazyload_images_after_onload(bool x) {
    set_option(x, &lazyload_images_after_onload_);
  }
  bool lazyload_images_after_onload() const {
    return lazyload_images_after_onload_.value();
  }

  void set_lazyload_images_blank_url(const StringPiece& p) {
    set_option(GoogleString(p.data(), p.size()), &lazyload_images_blank_url_);
  }
  const GoogleString& lazyload_images_blank_url() const {
    return lazyload_images_blank_url_.value();
  }

  void set_max_inlined_preview_images_index(int x) {
    set_option(x, &max_inlined_preview_images_index_);
  }
  int max_inlined_preview_images_index() const {
    return max_inlined_preview_images_index_.value();
  }

  void set_min_image_size_low_resolution_bytes(int64 x) {
    set_option(x, &min_image_size_low_resolution_bytes_);
  }
  int64 min_image_size_low_resolution_bytes() const {
    return min_image_size_low_resolution_bytes_.value();
  }

  void set_max_image_size_low_resolution_bytes(int64 x) {
    set_option(x, &max_image_size_low_resolution_bytes_);
  }
  int64 max_image_size_low_resolution_bytes() const {
    return max_image_size_low_resolution_bytes_.value();
  }

  void set_furious_cookie_duration_ms(int64 x) {
    set_option(x, &furious_cookie_duration_ms_);
  }
  int64 furious_cookie_duration_ms() const {
    return furious_cookie_duration_ms_.value();
  }

  void set_finder_properties_cache_expiration_time_ms(int64 x) {
    set_option(x, &finder_properties_cache_expiration_time_ms_);
  }
  int64 finder_properties_cache_expiration_time_ms() const {
    return finder_properties_cache_expiration_time_ms_.value();
  }

  void set_finder_properties_cache_refresh_time_ms(int64 x) {
    set_option(x, &finder_properties_cache_refresh_time_ms_);
  }
  int64 finder_properties_cache_refresh_time_ms() const {
    return finder_properties_cache_refresh_time_ms_.value();
  }
  bool css_preserve_urls() const {
    return css_preserve_urls_.value();
  }
  void set_css_preserve_urls(bool x) {
    set_option(x, &css_preserve_urls_);
  }

  bool image_preserve_urls() const {
    return image_preserve_urls_.value();
  }
  void set_image_preserve_urls(bool x) {
    set_option(x, &image_preserve_urls_);
  }

  bool js_preserve_urls() const {
    return js_preserve_urls_.value();
  }
  void set_js_preserve_urls(bool x) {
    set_option(x, &js_preserve_urls_);
  }

  bool image_retain_color_profile() const {
    return image_retain_color_profile_.value();
  }
  void set_image_retain_color_profile(bool x) {
    set_option(x, &image_retain_color_profile_);
  }

  bool image_retain_color_sampling() const {
    return image_retain_color_sampling_.value();
  }
  void set_image_retain_color_sampling(bool x) {
    set_option(x, &image_retain_color_sampling_);
  }

  bool image_retain_exif_data() const {
    return image_retain_exif_data_.value();
  }
  void set_image_retain_exif_data(bool x) {
    set_option(x, &image_retain_exif_data_);
  }

  void set_metadata_cache_staleness_threshold_ms(int64 x) {
    set_option(x, &metadata_cache_staleness_threshold_ms_);
  }
  int64 metadata_cache_staleness_threshold_ms() const {
    return metadata_cache_staleness_threshold_ms_.value();
  }

  void set_metadata_input_errors_cache_ttl_ms(int64 x) {
    set_option(x, &metadata_input_errors_cache_ttl_ms_);
  }
  int64 metadata_input_errors_cache_ttl_ms() const {
    return metadata_input_errors_cache_ttl_ms_.value();
  }

  const BeaconUrl& beacon_url() const { return beacon_url_.value(); }
  void set_beacon_url(const GoogleString& beacon_url) {
    beacon_url_.SetFromString(beacon_url);
  }

  // Return false in a subclass if you want to disallow all URL trimming in CSS.
  virtual bool trim_urls_in_css() const { return true; }

  int64 image_jpeg_recompress_quality() const {
    return image_jpeg_recompress_quality_.value();
  }
  void set_image_jpeg_recompress_quality(int64 x) {
    set_option(x, &image_jpeg_recompress_quality_);
  }

  int64 image_jpeg_recompress_quality_for_small_screens() const {
    return image_jpeg_recompress_quality_for_small_screens_.value();
  }
  void set_image_jpeg_recompress_quality_for_small_screens(int64 x) {
    set_option(x, &image_jpeg_recompress_quality_for_small_screens_);
  }

  int64 image_recompress_quality() const {
    return image_recompress_quality_.value();
  }
  void set_image_recompress_quality(int64 x) {
    set_option(x, &image_recompress_quality_);
  }

  int image_limit_optimized_percent() const {
    return image_limit_optimized_percent_.value();
  }
  void set_image_limit_optimized_percent(int x) {
    set_option(x, &image_limit_optimized_percent_);
  }
  int image_limit_resize_area_percent() const {
    return image_limit_resize_area_percent_.value();
  }
  void set_image_limit_resize_area_percent(int x) {
    set_option(x, &image_limit_resize_area_percent_);
  }

  int image_jpeg_num_progressive_scans() const {
    return image_jpeg_num_progressive_scans_.value();
  }
  void set_image_jpeg_num_progressive_scans(int x) {
    set_option(x, &image_jpeg_num_progressive_scans_);
  }

  int64 image_webp_recompress_quality() const {
    return image_webp_recompress_quality_.value();
  }
  void set_image_webp_recompress_quality(int64 x) {
    set_option(x, &image_webp_recompress_quality_);
  }

  int64 image_webp_recompress_quality_for_small_screens() const {
    return image_webp_recompress_quality_for_small_screens_.value();
  }
  void set_image_webp_recompress_quality_for_small_screens(int64 x) {
    set_option(x, &image_webp_recompress_quality_for_small_screens_);
  }
  int64 image_webp_timeout_ms() const {
    return image_webp_timeout_ms_.value();
  }
  void set_image_webp_timeout_ms(int64 x) {
    set_option(x, &image_webp_timeout_ms_);
  }

  bool domain_rewrite_hyperlinks() const {
    return domain_rewrite_hyperlinks_.value();
  }
  void set_domain_rewrite_hyperlinks(bool x) {
    set_option(x, &domain_rewrite_hyperlinks_);
  }

  bool client_domain_rewrite() const {
    return client_domain_rewrite_.value();
  }
  void set_client_domain_rewrite(bool x) {
    set_option(x, &client_domain_rewrite_);
  }

  void set_flush_more_resources_early_if_time_permits(bool x) {
    set_option(x, &flush_more_resources_early_if_time_permits_);
  }
  bool flush_more_resources_early_if_time_permits() const {
    return flush_more_resources_early_if_time_permits_.value();
  }

  void set_flush_more_resources_in_ie_and_firefox(bool x) {
    set_option(x, &flush_more_resources_in_ie_and_firefox_);
  }
  bool flush_more_resources_in_ie_and_firefox() const {
    return flush_more_resources_in_ie_and_firefox_.value();
  }

  void set_enable_defer_js_experimental(bool x) {
    set_option(x, &enable_defer_js_experimental_);
  }
  bool enable_defer_js_experimental() const {
    return enable_defer_js_experimental_.value();
  }

  void set_enable_inline_preview_images_experimental(bool x) {
    set_option(x, &enable_inline_preview_images_experimental_);
  }
  bool enable_inline_preview_images_experimental() const {
    return enable_inline_preview_images_experimental_.value();
  }

  void set_lazyload_highres_images(bool x) {
    set_option(x, &lazyload_highres_images_);
  }
  bool lazyload_highres_images() const {
    return lazyload_highres_images_.value();
  }

  void set_enable_blink_debug_dashboard(bool x) {
    set_option(x, &enable_blink_debug_dashboard_);
  }
  bool enable_blink_debug_dashboard() const {
    return enable_blink_debug_dashboard_.value();
  }

  void set_enable_blink_html_change_detection(bool x) {
    set_option(x, &enable_blink_html_change_detection_);
  }
  bool enable_blink_html_change_detection() const {
    return enable_blink_html_change_detection_.value();
  }

  void set_enable_blink_html_change_detection_logging(bool x) {
    set_option(x, &enable_blink_html_change_detection_logging_);
  }
  bool enable_blink_html_change_detection_logging() const {
    return enable_blink_html_change_detection_logging_.value();
  }

  void set_use_smart_diff_in_blink(bool x) {
    set_option(x, &use_smart_diff_in_blink_);
  }
  bool use_smart_diff_in_blink() const {
    return use_smart_diff_in_blink_.value();
  }

  void set_enable_lazyload_in_blink(bool x) {
    set_option(x, &enable_lazyload_in_blink_);
  }
  bool enable_lazyload_in_blink() const {
    return enable_lazyload_in_blink_.value();
  }

  void set_enable_prioritizing_scripts(bool x) {
    set_option(x, &enable_prioritizing_scripts_);
  }
  bool enable_prioritizing_scripts() const {
    return enable_prioritizing_scripts_.value();
  }

  void set_blink_html_change_detection_time_ms(int64 x) {
    set_option(x, &blink_html_change_detection_time_ms_);
  }
  int64 blink_html_change_detection_time_ms() const {
    return blink_html_change_detection_time_ms_.value();
  }

  void set_override_blink_cache_time_ms(int64 x) {
    set_option(x, &override_blink_cache_time_ms_);
  }
  int64 override_blink_cache_time_ms() const {
    return override_blink_cache_time_ms_.value();
  }

  void set_blink_non_cacheables_for_all_families(const StringPiece& p) {
    set_option(GoogleString(p.data(), p.size()),
               &blink_non_cacheables_for_all_families_);
  }
  const GoogleString& blink_non_cacheables_for_all_families() const {
    return blink_non_cacheables_for_all_families_.value();
  }

  const GoogleString& blocking_rewrite_key() const {
    return blocking_rewrite_key_.value();
  }
  void set_blocking_rewrite_key(const StringPiece& p) {
    set_option(p.as_string(), &blocking_rewrite_key_);
  }

  bool rewrite_uncacheable_resources() const {
    return rewrite_uncacheable_resources_.value();
  }

  void set_rewrite_uncacheable_resources(bool x) {
    set_option(x, &rewrite_uncacheable_resources_);
  }

  // Does url match a cacheable family pattern?  Returns true if url matches a
  // url_pattern in prioritize_visible_content_families_.
  bool IsInBlinkCacheableFamily(const GoogleUrl& gurl) const;

  // Get the cache time for gurl for prioritize_visible_content filter.  In case
  // gurl matches a url_pattern in prioritize_visible_content_families_ we
  // return the corresponding cache_time_ms field, else we return
  // kDefaultPrioritizeVisibleContentCacheTimeMs.
  int64 GetBlinkCacheTimeFor(const GoogleUrl& gurl) const;

  // Get elements to be treated as non-cacheable for gurl.  In case
  // gurl matches a url_pattern in prioritize_visible_content_families_ we
  // return the corresponding non_cacheable_elements field, else we return empty
  // string.
  GoogleString GetBlinkNonCacheableElementsFor(const GoogleUrl& gurl) const;

  // Create and add a PrioritizeVisibleContentFamily object the given fields.
  void AddBlinkCacheableFamily(const StringPiece url_pattern,
                               int64 cache_time_ms,
                               const StringPiece non_cacheable_elements);

  void set_running_furious_experiment(bool x) {
    set_option(x, &running_furious_);
  }
  bool running_furious() const {
    return running_furious_.value();
  }

  // x should be between 1 and 5 inclusive.
  void set_furious_ga_slot(int x) {
    set_option(x, &furious_ga_slot_);
  }

  int furious_ga_slot() const { return furious_ga_slot_.value(); }

  void set_report_unload_time(bool x) {
    set_option(x, &report_unload_time_);
  }
  bool report_unload_time() const {
    return report_unload_time_.value();
  }

  void set_implicit_cache_ttl_ms(int64 x) {
    set_option(x, &implicit_cache_ttl_ms_);
  }
  int64 implicit_cache_ttl_ms() const {
    return implicit_cache_ttl_ms_.value();
  }

  void set_x_header_value(const StringPiece& p) {
    set_option(p.as_string(), &x_header_value_);
  }
  const GoogleString& x_header_value() const {
    return x_header_value_.value();
  }

  void set_distributed_rewrite_servers(const StringPiece& p) {
      set_option(p.as_string(), &distributed_rewrite_servers_);
  }
  const GoogleString& distributed_rewrite_servers() const {
    return distributed_rewrite_servers_.value();
  }

  void set_distributed_rewrite_timeout_ms(const int64 x) {
    set_option(x, &distributed_rewrite_timeout_ms_);
  }
  int64 distributed_rewrite_timeout_ms() const {
    return distributed_rewrite_timeout_ms_.value();
  }

  void set_avoid_renaming_introspective_javascript(bool x) {
    set_option(x, &avoid_renaming_introspective_javascript_);
  }
  bool avoid_renaming_introspective_javascript() const {
    return avoid_renaming_introspective_javascript_.value();
  }

  void set_passthrough_blink_for_last_invalid_response_code(bool x) {
    set_option(x, &passthrough_blink_for_last_invalid_response_code_);
  }
  bool passthrough_blink_for_last_invalid_response_code() const {
    return passthrough_blink_for_last_invalid_response_code_.value();
  }

  int64 blink_max_html_size_rewritable() const {
    return blink_max_html_size_rewritable_.value();
  }
  void set_blink_max_html_size_rewritable(int64 x) {
    set_option(x, &blink_max_html_size_rewritable_);
  }

  void set_apply_blink_if_no_families(bool x) {
    set_option(x, &apply_blink_if_no_families_);
  }
  bool apply_blink_if_no_families() const {
    return apply_blink_if_no_families_.value();
  }

  void set_critical_line_config(const StringPiece& p) {
      set_option(GoogleString(p.data(), p.size()), &critical_line_config_);
  }
  const GoogleString& critical_line_config() const {
    return critical_line_config_.value();
  }

  void set_forbid_all_disabled_filters(bool x) {
    set_option(x, &forbid_all_disabled_filters_);
  }
  bool forbid_all_disabled_filters() const {
    return forbid_all_disabled_filters_.value();
  }

  bool reject_blacklisted() const { return reject_blacklisted_.value(); }
  void set_reject_blacklisted(bool x) {
    set_option(x, &reject_blacklisted_);
  }

  HttpStatus::Code reject_blacklisted_status_code() const {
    return static_cast<HttpStatus::Code>(
        reject_blacklisted_status_code_.value());
  }
  void set_reject_blacklisted_status_code(HttpStatus::Code x) {
    set_option(static_cast<int>(x), &reject_blacklisted_status_code_);
  }

  bool support_noscript_enabled() const {
    return support_noscript_enabled_.value();
  }
  void set_support_noscript_enabled(bool x) {
    set_option(x, &support_noscript_enabled_);
  }

  void set_max_combined_js_bytes(int64 x) {
    set_option(x, &max_combined_js_bytes_);
  }
  int64 max_combined_js_bytes() const {
    return max_combined_js_bytes_.value();
  }

  void set_pre_connect_url(const StringPiece& p) {
    set_option(GoogleString(p.data(), p.size()), &pre_connect_url_);
  }
  const GoogleString& pre_connect_url() const {
    return pre_connect_url_.value();
  }
  void set_property_cache_http_status_stability_threshold(int x) {
    set_option(x, &property_cache_http_status_stability_threshold_);
  }
  int property_cache_http_status_stability_threshold() const {
    return property_cache_http_status_stability_threshold_.value();
  }

  void set_max_rewrite_info_log_size(int x) {
    set_option(x, &max_rewrite_info_log_size_);
  }
  int max_rewrite_info_log_size() const {
    return max_rewrite_info_log_size_.value();
  }

  void set_enable_aggressive_rewriters_for_mobile(bool x) {
    set_option(x, &enable_aggressive_rewriters_for_mobile_);
  }
  bool enable_aggressive_rewriters_for_mobile() const {
    return enable_aggressive_rewriters_for_mobile_.value();
  }

  void set_allow_logging_urls_in_log_record(bool x) {
    set_option(x, &allow_logging_urls_in_log_record_);
  }
  bool allow_logging_urls_in_log_record() const {
    return allow_logging_urls_in_log_record_.value();
  }

  // Merge src into 'this'.  Generally, options that are explicitly
  // set in src will override those explicitly set in 'this' (except that
  // filters forbidden in 'this' cannot be enabled by 'src'), although
  // option Merge implementations can be redefined by specific Option
  // class implementations (e.g. OptionInt64MergeWithMax).  One
  // semantic subject to interpretation is when a core-filter is
  // disabled in the first set and not in the second.  My judgement is
  // that the 'disable' from 'this' should override the core-set
  // membership in the 'src', but not an 'enable' in the 'src'.
  //
  // You can make an exact duplicate of RewriteOptions object 'src' via
  // (new 'typeof src')->Merge(src), aka Clone().
  //
  // Merge expects that 'src' and 'this' are the same type.  If that's
  // not true, this function will DCHECK.
  virtual void Merge(const RewriteOptions& src);

  // Registers a wildcard pattern for to be allowed, potentially overriding
  // previous Disallow wildcards.
  void Allow(const StringPiece& wildcard_pattern) {
    Modify();
    allow_resources_.Allow(wildcard_pattern);
  }

  // Registers a wildcard pattern for to be disallowed, potentially overriding
  // previous Allow wildcards.
  void Disallow(const StringPiece& wildcard_pattern) {
    Modify();
    allow_resources_.Disallow(wildcard_pattern);
  }

  // Blacklist of javascript files that don't like their names changed.
  // This should be called for root options to set defaults.
  // TODO(sligocki): Rename to allow for more general initialization.
  virtual void DisallowTroublesomeResources();

  // Disallows resources that are served on well-distributed CDNs
  // already, and are likely to be in browser-caches, or that are
  // troublesome resources stored on external domains.  Note: this is
  // not currently called by mod_pagespeed.
  virtual void DisallowResourcesForProxy();

  DomainLawyer* domain_lawyer() { return &domain_lawyer_; }
  const DomainLawyer* domain_lawyer() const { return &domain_lawyer_; }

  FileLoadPolicy* file_load_policy() { return &file_load_policy_; }
  const FileLoadPolicy* file_load_policy() const { return &file_load_policy_; }

  // Determines, based on the sequence of Allow/Disallow calls above, whether
  // a url is allowed.
  bool IsAllowed(const StringPiece& url) const {
    return allow_resources_.Match(url, true);
  }

  // Adds a new comment wildcard pattern to be retained.
  void RetainComment(const StringPiece& comment) {
    Modify();
    retain_comments_.Allow(comment);
  }

  // If enabled, the 'remove_comments' filter will remove all HTML comments.
  // As discussed in Issue 237, some comments have semantic value and must
  // be retained.
  bool IsRetainedComment(const StringPiece& comment) const {
    return retain_comments_.Match(comment, false);
  }

  // Adds a new class name for which lazyload should be disabled.
  void DisableLazyloadForClassName(const StringPiece& class_name) {
    Modify();
    lazyload_enabled_classes_.Disallow(class_name);
  }

  // Checks if lazyload images is enabled for the specified class.
  bool IsLazyloadEnabledForClassName(const StringPiece& class_name) const {
    return lazyload_enabled_classes_.Match(class_name, true);
  }

  void set_override_caching_ttl_ms(int64 x) {
    set_option(x, &override_caching_ttl_ms_);
  }
  int64 override_caching_ttl_ms() const {
    return override_caching_ttl_ms_.value();
  }

  // Overrides the cache ttl for all urls matching the wildcard with
  // override_caching_ttl_ms().
  void AddOverrideCacheTtl(const StringPiece& wildcard) {
    Modify();
    override_caching_wildcard_.Allow(wildcard);
  }

  // Is the cache TTL overridden for the given url?
  bool IsCacheTtlOverridden(const StringPiece& url) const {
    return override_caching_wildcard_.Match(url, false);
  }

  void AddRejectedUrlWildcard(const GoogleString& wildcard) {
    AddRejectedHeaderWildcard(kRejectedRequestUrlKeyName, wildcard);
  }

  void AddRejectedHeaderWildcard(const StringPiece& header_name,
                                 const GoogleString& wildcard) {
    Modify();
    std::pair<FastWildcardGroupMap::iterator, bool> insert_result =
        rejected_request_map_.insert(std::make_pair(
            header_name, static_cast<FastWildcardGroup*>(NULL)));

    if (insert_result.second) {
      insert_result.first->second = new FastWildcardGroup;
    }
    insert_result.first->second->Allow(wildcard);
  }

  bool IsRejectedUrl(const GoogleString& url) const {
    return IsRejectedRequest(kRejectedRequestUrlKeyName, url);
  }

  bool IsRejectedRequest(const StringPiece& header_name,
                         const StringPiece& value) const {
    FastWildcardGroupMap::const_iterator it = rejected_request_map_.find(
        header_name);
    if (it != rejected_request_map_.end()) {
      return it->second->Match(value, false);
    }
    return false;
  }
  // Make an identical copy of these options and return it.  This does
  // *not* copy the signature, and the returned options are not in
  // a frozen state.
  virtual RewriteOptions* Clone() const;

  // Computes a signature for the RewriteOptions object, including all
  // contained classes (DomainLawyer, FileLoadPolicy, WildCardGroups).
  //
  // Computing a signature "freezes" the class instance.  Attempting
  // to modify a RewriteOptions after freezing will DCHECK.
  void ComputeSignature(const Hasher* hasher);

  // Clears the computed signature, unfreezing the options object.
  // Warning: Please note that using this method is extremely risky and should
  // be avoided as much as possible. If you are planning to use this, please
  // discuss this with your team-mates and ensure that you clearly understand
  // its implications. Also, please do repeat this warning at every place you
  // use this method.
  void ClearSignatureWithCaution() {
    frozen_ = false;
    signature_.clear();
  }

  // Clears a computed signature, unfreezing the options object.  This
  // is intended for testing.
  void ClearSignatureForTesting() {
    ClearSignatureWithCaution();
  }

  // Returns the computed signature.
  const GoogleString& signature() const {
    // We take a reader-lock because we may be looking at the
    // global_options signature concurrent with updating it if someone
    // flushes cache.  Note that the default mutex implementation is
    // NullRWLock, which isn't actually a mutex.  Only (currently) for the
    // Apache global_options() object do we create a real mutex.  We
    // don't expect contention here because we take a reader-lock and the
    // only time we Write is if someone flushes the cache.
    ThreadSystem::ScopedReader lock(cache_invalidation_timestamp_.mutex());
    DCHECK(frozen_);
    return signature_;
  }

  virtual GoogleString OptionsToString() const;

  // Returns a string identifying the currently running Furious experiment to
  // be used in tagging Google Analytics data.
  virtual GoogleString ToExperimentString() const;

  // Returns a string with more information about the currently running furious
  // experiment. Primarily used for tagging Google Analytics data.  This format
  // is not at all specific to Google Analytics, however.
  virtual GoogleString ToExperimentDebugString() const;

  // Returns true if generation low res images is required.
  virtual bool NeedLowResImages() const {
    return Enabled(kDelayImages);
  }

  // Convert an id string like "ah" to a Filter enum like kAddHead.
  // Returns kEndOfFilters if the id isn't known.
  static Filter LookupFilterById(const StringPiece& filter_id);

  // Looks up an option id and returns the corresponding enum, or kEndOfOptions
  // if the id is not found.  Example, takes "ii" and returns
  // kDefaultImageInlineMaxBytes.
  static OptionEnum LookupOptionEnumById(const StringPiece& option_id);

  // Returns the option name corresponding to the option enum.
  static const char* LookupOptionEnum(OptionEnum option_enum) {
    return (option_enum < kEndOfOptions) ?
        option_enum_to_name_array_[option_enum] : NULL;
  }

  static OptionEnum LookupOption(const StringPiece& option_name);

  // Return the list of all options.  Used to initialize the configuration
  // vector to the Apache configuration system.
  const OptionBaseVector& all_options() const {
    return all_options_;
  }

 protected:
  // Type-specific class of Property.  This subclass of PropertyBase
  // knows what sort of value the Option will hold, and so we can put
  // the default value here.
  template<class ValueType>
  class Property : public PropertyBase {
   public:
    // When adding a new Property, we take the default_value by value,
    // not const-reference.  This is because when calling AddProperty
    // we may want to use a compile-time constant
    // (e.g. Timer::kHourMs) which does not have a linkable address.
    Property(ValueType default_value,
             const char* id,
             OptionEnum option_enum)
        : PropertyBase(id, option_enum),
          default_value_(default_value) {
    }

    void set_default(ValueType value) { default_value_ = value; }
    const ValueType& default_value() const { return default_value_; }

   private:
    ValueType default_value_;

    DISALLOW_COPY_AND_ASSIGN(Property);
  };

  // Leaf subclass of Property<ValueType>, which is templated on the class of
  // the corresponding Option.  The template parameters here are
  //   RewriteOptionsSubclass -- the subclass of RewriteOptions in which
  //     this option is instantiated, e.g. ApacheConfig.
  //   OptionClass -- the subclass of OptionBase that is being instantiated
  //     in each RewriteOptionsSubclass.
  // These template parameters are generally automatically discovered by
  // the compiler when AddProperty is called.
  //
  // TODO(jmarantz): It looks tempting to fold Property<T> and
  // PropertyLeaf<T> together, but this is difficult because of the
  // way that the Option class hiearchy is structured and the
  // precision of C++ pointers-to-members.  Attempting that is
  // probably a worthwhile follow-up task.
  template<class RewriteOptionsSubclass, class OptionClass>
  class PropertyLeaf : public Property<typename OptionClass::ValueType> {
   public:
    // Fancy C++ pointers to members; a typesafe version of offsetof.  See
    // http://publib.boulder.ibm.com/infocenter/comphelp/v8v101/index.jsp?
    // topic=%2Fcom.ibm.xlcpp8a.doc%2Flanguage%2Fref%2Fcplr034.htm
    typedef OptionClass RewriteOptionsSubclass::*OptionOffset;
    typedef typename OptionClass::ValueType ValueType;

    PropertyLeaf(ValueType default_value,
                 OptionOffset offset,
                 const char* id,
                 OptionEnum option_enum)
        : Property<ValueType>(default_value, id, option_enum),
          offset_(offset) {
    }

    virtual void InitializeOption(RewriteOptions* options) const {
      RewriteOptionsSubclass* options_subclass =
          static_cast<RewriteOptionsSubclass*>(options);
      OptionClass& option = options_subclass->*offset_;
      option.set_property(this);
      DCHECK_NE(-1, this->index()) << "Call Property::set_index first.";
      options->set_option_at(this->index(), &option);
    }

   private:
    OptionOffset offset_;

    DISALLOW_COPY_AND_ASSIGN(PropertyLeaf);
  };

  // Helper class to represent an Option, whose value is held in some class T.
  // An option is explicitly initialized with its default value, although the
  // default value can be altered later.  It keeps track of whether a
  // value has been explicitly set (independent of whether that happens to
  // coincide with the default value).
  //
  // It can use this knowledge to intelligently merge a 'base' option value
  // into a 'new' option value, allowing explicitly set values from 'base'
  // to override default values from 'new'.
  template<class T> class OptionTemplateBase : public OptionBase {
   public:
    typedef T ValueType;

    OptionTemplateBase() : was_set_(false), property_(NULL) {}

    virtual bool was_set() const { return was_set_; }

    void set(const T& val) {
      was_set_ = true;
      value_ = val;
    }

    void set_default(const T& val) {
      if (!was_set_) {
        value_ = val;
      }
    }

    const T& value() const { return value_; }

    // The signature of the Merge implementation must match the base-class.  The
    // caller is responsible for ensuring that only the same typed Options are
    // compared.  In RewriteOptions::Merge this is guaranteed because the
    // vector<OptionBase*> all_options_ is sorted on option_enum().  We DCHECK
    // that the option_enum of this and src are the same.
    virtual void Merge(const OptionBase* src) {
      DCHECK(option_enum() == src->option_enum());
      MergeHelper(static_cast<const OptionTemplateBase*>(src));
    }

    void MergeHelper(const OptionTemplateBase* src) {
      // Even if !src->was_set, the default value needs to be transferred
      // over in case it was changed with set_default or SetDefaultRewriteLevel.
      if (src->was_set_ || !was_set_) {
        value_ = src->value_;
        was_set_ = src->was_set_;
      }
    }

    // The static properties of an Option are held in a Property<T>*.
    void set_property(const Property<T>* property) {
      property_ = property;

      // Note that the copying of default values here is only required
      // to support SetDefaultRewriteLevel, which it should be
      // possible to remove.  Otherwise we could just pull the
      // default value out of properties_ when !was_set_;
      value_ = property->default_value();
    }
    virtual const Property<T>* property() const { return property_; }

    // Sets a the option default value globally.  This is thread-unsafe,
    // and reaches into the option property_ field via a const-cast to
    // mutate it.  Note that this method does not affect the current value
    // of the instantiated option.
    //
    // TODO(jmarantz): consider an alternate structure where the
    // Property<T>* can be easily located programmatically
    // rather than going through a dummy Option object.
    void set_global_default(const T& val) {
      Property<T>* property = const_cast<Property<T>*>(property_);
      property->set_default(val);
    }

    // Sets a the option's participation in Signatures globally.  This
    // is thread-unsafe, and reaches into the option property_ field
    // via a const-cast to mutate it.  Note that this method does not
    // affect the current value of the instantiated option.
    //
    // TODO(jmarantz): consider an alternate structure where the
    // Property<T>* can be easily located programmatically
    // rather than going through a dummy Option object.
    void DoNotUseForSignatureComputation() {
      Property<T>* property = const_cast<Property<T>*>(property_);
      property->set_do_not_use_for_signature_computation(true);
    }

   private:
    bool was_set_;
    T value_;
    const Property<T>* property_;

    DISALLOW_COPY_AND_ASSIGN(OptionTemplateBase);
  };

  // Subclassing OptionTemplateBase so that the conversion functions that need
  // to invoke static overloaded functions are declared only here.  Enables
  // subclasses of RewriteOptions to override these in case they use Option
  // types not visible here.
  template<class T> class Option : public OptionTemplateBase<T> {
   public:
    Option() {}

    // Sets value_ from value_string.
    virtual bool SetFromString(const GoogleString& value_string) {
      T value;
      bool success = RewriteOptions::ParseFromString(value_string, &value);
      if (success) {
        this->set(value);
      }
      return success;
    }

    virtual GoogleString Signature(const Hasher* hasher) const {
      return RewriteOptions::OptionSignature(this->value(), hasher);
    }

    virtual GoogleString ToString() const {
      return RewriteOptions::ToString(this->value());
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Option);
  };

  // Like Option<int64>, but merge by taking the Max of the two values.  Note
  // that this could be templatized on type in which case we'd need to inline
  // the implementation of Merge.
  //
  // This class has an optional mutex for allowing Apache to flush cache by
  // mutating its global_options().  Note that global_options() is never used
  // directly in a rewrite_driver, but is cloned with this optional Mutex held.
  //
  // The "optional" mutex is always present, but it defaults to a
  // NullRWLock, which has empty implementations of all
  // locking/unlocking functions.  Only in Apache (currently) do we
  // override that with a real RWLock from the thread system.
  class MutexedOptionInt64MergeWithMax : public Option<int64> {
   public:
    MutexedOptionInt64MergeWithMax();
    virtual ~MutexedOptionInt64MergeWithMax();

    // Merges src_base into this by taking the maximum of the two values.
    //
    // We expect ot have exclusive access to 'this' and don't need to lock it,
    // but we use locked access to src_base->value().
    virtual void Merge(const OptionBase* src_base);

    // The value() must only be taken when the mutex is held.  This is
    // only called by RewriteOptions::UpdateCacheInvalidationTimestampMs
    // and MutexedOptionInt64MergeWithMax::Merge, which are holding
    // locks when calling value().
    //
    // Note that we don't require or take the lock for set(), so we
    // don't override set.  When updating or merging, we already have
    // a lock and can't take it again.  When writing the invalidation
    // timestamp at initial configuration time, we don't need the
    // lock.
    void checked_set(const int64& value) {
      mutex_->DCheckLocked();
      Option<int64>::set(value);
    }

    // Returns the mutex for this object.  When this class is
    // constructed it gets a NullRWLock which doesn't actually lock
    // anything.  This is because we generally initialize
    // RewriteOptions from only one thread, and thereafter do only
    // reads.  However, one exception is the cache-invalidation
    // timestamp in the global_options for Apache ResourceManagers,
    // which can be written from any thread handling a request,
    // particularly with the Worker MPM.  So we install a real RWLock*
    // for Apache's global_options.
    //
    // Also note that this mutex, when installed, is also used to
    // lock access to RewriteOptions::signature(), which depends on
    // the cache invalidation timestamp.
    ThreadSystem::RWLock* mutex() const { return mutex_.get(); }

    // Takes ownership of mutex.  Note that by default, mutex()
    // has a NullRWLock.  Only by calling set_mutex do we add locking
    // semantics for the invalidation timestamp & signature.  If
    // we allow other settings to be spontaneously changed we will
    // have to add further locking.
    void set_mutex(ThreadSystem::RWLock* lock) { mutex_.reset(lock); }

   private:
    scoped_ptr<ThreadSystem::RWLock> mutex_;
  };

 protected:
  // Adds a new Property to 'properties' (the last argument).
  template<class RewriteOptionsSubclass, class OptionClass>
  static PropertyBase* AddProperty(
      typename OptionClass::ValueType default_value,
      OptionClass RewriteOptionsSubclass::*offset,
      const char* id,
      OptionEnum option_enum,
      OptionScope scope,
      const char* help_text,
      Properties* properties) {
    PropertyBase* property =
        new PropertyLeaf<RewriteOptionsSubclass, OptionClass>(
            default_value, offset, id, option_enum);
    property->set_scope(scope);
    property->set_help_text(help_text);
    properties->push_back(property);
    return property;
  }

  // Merges properties into all_properties so that
  // RewriteOptions::Merge and SetOptionFromName can work across
  // options from RewriteOptions and all relevant subclasses.
  //
  // Each RewriteOptions subclass keeps its own property lists using
  // its own private Properties* member variables.  The private lists
  // are used for initialization of default-values during
  // construction.  We cannot initialize subclass default option
  // values during RewriteOptions construction because options with
  // non-POD ValueType (e.g. GoogleString) have not yet been
  // initialized, so we have to keep separate per-class property-lists
  // for use during construction.  However, we use a global sorted
  // list for fast merging and setting-by-option-name.
  static void MergeSubclassProperties(Properties* properties);

  // Forbid filters that PreserveUrls is incompatible with.
  void ForbidFiltersForPreserveUrl();

  // Populates all_options_, based on the passed-in index, which
  // should correspond to the property index calculated after
  // sorting all_properties_.  This enables us to sort the all_properties_
  // vector once, and use that to give us all_options_ that is sorted
  // the same way.
  void set_option_at(int index, OptionBase* option) {
    all_options_[index] = option;
  }

  // When setting an option, however, we generally are doing so
  // with a variable rather than a constant so it makes sense to pass
  // it by reference.
  template<class T>
  void set_option(const T& new_value, OptionTemplateBase<T>* option) {
    option->set(new_value);
    Modify();
  }

  // Marks the config as modified.
  void Modify();

  // Sets the global default value for 'x_header_value'.  Note that setting
  // this Option reaches through to the underlying property and sets the
  // default value there, and in fact does *not affect the value of the
  // instantiated RewriteOptions object.
  //
  // TODO(jmarantz): Remove this method and make another one that operate
  // directly on the Property.
  void set_default_x_header_value(const StringPiece& x_header_value) {
    x_header_value_.set_global_default(x_header_value.as_string());
  }

  // Enable/disable filters and set options according to the current FuriousSpec
  // that furious_id_ matches. Returns true if the state was set successfully.
  bool SetupFuriousRewriters();

  // Enables filters needed by Furious regardless of experiment.
  virtual void SetRequiredFuriousFilters();

  // Helper method to add pre-configured FuriousSpec objects to the internal
  // vector of FuriousSpec's. Returns true if the experiment was added
  // successfully. Takes ownership of (and may delete) spec.
  bool InsertFuriousSpecInVector(FuriousSpec* spec);

  // Protected option values so that derived class can modify.
  Option<BeaconUrl> beacon_url_;

  // The value we put for the X-Mod-Pagespeed header. Default is our version.
  Option<GoogleString> x_header_value_;

 private:
  struct OptionIdCompare;

  static Properties* properties_;          // from RewriteOptions only
  static Properties* all_properties_;      // includes subclass properties

  FRIEND_TEST(RewriteOptionsTest, FuriousMergeTest);
  typedef std::vector<Filter> FilterVector;

  // A family of urls for which prioritize_visible_content filter can be
  // applied.  url_pattern represents the actual set of urls,
  // cache_time_ms is the duration for which the cacheable portions of pages of
  // the family can be cached, and non_cacheable_elements is a comma-separated
  // list of elements (e.g., "id:foo,class:bar") that cannot be cached for the
  // family.
  struct PrioritizeVisibleContentFamily {
    PrioritizeVisibleContentFamily(StringPiece url_pattern_string,
                                   int64 cache_time_ms_in,
                                   StringPiece non_cacheable_elements_in)
        : url_pattern(url_pattern_string),
          cache_time_ms(cache_time_ms_in),
          non_cacheable_elements(non_cacheable_elements_in.data(),
                                 non_cacheable_elements_in.size()) {}

    PrioritizeVisibleContentFamily* Clone() const {
      return new PrioritizeVisibleContentFamily(
          url_pattern.spec(), cache_time_ms, non_cacheable_elements);
    }

    GoogleString ComputeSignature() const {
      return StrCat(url_pattern.spec(), ";", Integer64ToString(cache_time_ms),
                    ";", non_cacheable_elements);
    }

    GoogleString ToString() const {
      return StrCat("URL pattern: ", url_pattern.spec(), ",  Cache time (ms): ",
                    Integer64ToString(cache_time_ms), ",  Non-cacheable: ",
                    non_cacheable_elements);
    }

    Wildcard url_pattern;
    int64 cache_time_ms;
    GoogleString non_cacheable_elements;
  };

  // A URL pattern cache invalidation entry.  All values cached for an URL that
  // matches url_pattern before timestamp_ms should be evicted.
  struct UrlCacheInvalidationEntry {
    UrlCacheInvalidationEntry(StringPiece url_pattern_in,
                              int64 timestamp_ms_in,
                              bool is_strict_in)
        : url_pattern(url_pattern_in),
          timestamp_ms(timestamp_ms_in),
          is_strict(is_strict_in) {}

    UrlCacheInvalidationEntry* Clone() const {
      return new UrlCacheInvalidationEntry(
          url_pattern.spec(), timestamp_ms, is_strict);
    }

    GoogleString ComputeSignature() const {
      if (is_strict) {
        return "";
      }
      return StrCat(url_pattern.spec(), "@", Integer64ToString(timestamp_ms));
    }

    GoogleString ToString() const {
      return StrCat(
          url_pattern.spec(), ", ", (is_strict ? "STRICT" : "REFERENCE"), " @ ",
          Integer64ToString(timestamp_ms));
    }

    Wildcard url_pattern;
    int64 timestamp_ms;
    bool is_strict;
  };

  typedef std::vector<UrlCacheInvalidationEntry*>
      UrlCacheInvalidationEntryVector;

  // Private methods to help add properties to
  // RewriteOptions::properties_.  Subclasses define their own
  // versions of these to add to their own private property-lists, and
  // subsequently merge them into RewriteOptions::all_properties_ via
  // MergeSubclassProperties.
  //
  // This version a property without a unique option_enum_ field.
  // kEndOfOptions will be used for the enum, and thus
  // SetOptionFromName cannot be used for options associated with such
  // properties.
  //
  // TODO(jmarantz): This method should be removed and such properties
  // should be moved into RequestContext.
  template<class OptionClass, class RewriteOptionsSubclass>
  static void AddRequestProperty(typename OptionClass::ValueType default_value,
                                 OptionClass RewriteOptionsSubclass::*offset,
                                 const char* id) {
    AddProperty(default_value, offset, id, kEndOfOptions, kProcessScope,
                NULL, properties_);
  }

  // Adds a property with a unique option_enum_ field, allowing use of
  // SetOptionFromName.
  template<class RewriteOptionsSubclass, class OptionClass>
  static void AddBaseProperty(typename OptionClass::ValueType default_value,
                              OptionClass RewriteOptionsSubclass::*offset,
                              const char* id,
                              OptionEnum option_enum,
                              OptionScope scope,
                              const char* help) {
    AddProperty(default_value, offset, id, option_enum, scope, help,
                properties_);
  }

  static void AddProperties();
  bool AddCommaSeparatedListToFilterSetState(
      const StringPiece& filters, FilterSet* set, MessageHandler* handler);
  static bool AddCommaSeparatedListToFilterSet(
      const StringPiece& filters, FilterSet* set, MessageHandler* handler);
  static Filter LookupFilter(const StringPiece& filter_name);
  // Fix any option conflicts (e.g., if two options are mutually exclusive, then
  // disable one.)
  void ResolveConflicts();
  // Initialize the option-enum to option-name array for fast lookups by
  // OptionEnum.
  static void InitOptionEnumToNameArray();
  // Initialize the Filter id to enum reverse array used for fast lookups.
  static void InitFilterIdToEnumArray();
  static void InitOptionIdToEnumArray();
  // If str match a cacheable family pattern then returns the
  // PrioritizeVisibleContentFamily that it matches, else returns NULL.
  const PrioritizeVisibleContentFamily* FindPrioritizeVisibleContentFamily(
      const StringPiece str) const;

  // Helper for converting the result of SetOptionFromEnum into
  // a status/message pair. The returned result may be adjusted from the passed
  // in one (in particular when option_enum is kEndOfOptions).
  OptionSettingResult FormatSetOptionMessage(
      OptionSettingResult result, OptionEnum option_enum, StringPiece name,
      StringPiece value, GoogleString* msg);

  // These static methods enable us to generate signatures for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString OptionSignature(bool x, const Hasher* hasher) {
    return x ? "T" : "F";
  }
  static GoogleString OptionSignature(int x, const Hasher* hasher) {
    return IntegerToString(x);
  }
  static GoogleString OptionSignature(int64 x, const Hasher* hasher) {
    return Integer64ToString(x);
  }
  static GoogleString OptionSignature(const GoogleString& x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(RewriteLevel x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(const BeaconUrl& beacon_url,
                                      const Hasher* hasher);

  // These static methods enable us to generate strings for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString ToString(bool x) {
    return x ? "True" : "False";
  }
  static GoogleString ToString(int x) {
    return IntegerToString(x);
  }
  static GoogleString ToString(int64 x) {
    return Integer64ToString(x);
  }
  static GoogleString ToString(const GoogleString& x) {
    return x;
  }
  static GoogleString ToString(RewriteLevel x);
  static GoogleString ToString(const BeaconUrl& beacon_url);

  // Returns true if p1's's enum is less than p2's. Used to order
  // all_properties_.
  static bool PropertyLessThanByEnum(PropertyBase* p1, PropertyBase* p2) {
    return p1->option_enum() < p2->option_enum();
  }

  // Returns true if option's enum is less than arg.
  static bool OptionEnumLessThanArg(OptionBase* option, OptionEnum arg) {
    return option->option_enum() < arg;
  }

  // Returns true if e1's timestamp is less than e2's.
  static bool CompareUrlCacheInvalidationEntry(UrlCacheInvalidationEntry* e1,
                                               UrlCacheInvalidationEntry* e2) {
    return e1->timestamp_ms < e2->timestamp_ms;
  }

  // Returns true if the first entry's id is less than the second's id.
  static bool FilterEnumToIdAndNameEntryLessThanById(
      const FilterEnumToIdAndNameEntry* e1,
      const FilterEnumToIdAndNameEntry* e2) {
    return strcmp(e1->filter_id, e2->filter_id) < 0;
  }

  bool modified_;
  bool frozen_;
  FilterSet enabled_filters_;
  FilterSet disabled_filters_;
  FilterSet forbidden_filters_;

  // The set of filters that can be distributed to other tasks.
  // For experimentation, may be removed later.
  FilterSet distributable_filters_;

  // Note: using the template class Option here saves a lot of repeated
  // and error-prone merging code.  However, it is not space efficient as
  // we are alternating int64s and bools in the structure.  If we cared
  // about that, then we would keep the bools in a bitmask.  But since
  // we don't really care we'll try to keep the code structured better.
  Option<RewriteLevel> level_;

  // List of URL patterns and timestamp for which it should be invalidated.  In
  // increasing order of timestamp.
  UrlCacheInvalidationEntryVector url_cache_invalidation_entries_;

  MutexedOptionInt64MergeWithMax cache_invalidation_timestamp_;
  Option<int64> css_flatten_max_bytes_;
  Option<bool> cache_small_images_unrewritten_;
  // Sets limit for image optimization
  Option<int64> image_resolution_limit_bytes_;
  Option<int64> css_image_inline_max_bytes_;
  Option<int64> css_inline_max_bytes_;
  Option<int64> css_outline_min_bytes_;

  // Preserve URL options
  Option<bool> css_preserve_urls_;
  Option<bool> js_preserve_urls_;
  Option<bool> image_preserve_urls_;

  Option<int64> image_inline_max_bytes_;
  Option<int64> js_inline_max_bytes_;
  Option<int64> js_outline_min_bytes_;
  Option<int64> progressive_jpeg_min_bytes_;
  // The max Cache-Control TTL for HTML.
  Option<int64> max_html_cache_time_ms_;
  // The maximum number of bytes of HTML that we parse, before redirecting to
  // ?ModPagespeed=off.
  Option<int64> max_html_parse_bytes_;
  // The maximum size of an image in CSS, which we convert to webp.
  Option<int64> max_image_bytes_for_webp_in_css_;
  // Resources with Cache-Control TTL less than this will not be rewritten.
  Option<int64> min_resource_cache_time_to_rewrite_ms_;
  Option<int64> idle_flush_time_ms_;
  Option<int64> flush_buffer_limit_bytes_;

  // How long to wait in blocking fetches before timing out.
  // Applies to ResourceFetch::BlockingFetch() and class SyncFetcherAdapter.
  // Does not apply to async fetches.
  Option<int64> blocking_fetch_timeout_ms_;

  // Option related to generic image quality. This is overridden by
  // image(jpeg/webp) specific options.
  Option<int64> image_recompress_quality_;

  // Options related to jpeg compression.
  Option<int64> image_jpeg_recompress_quality_;
  Option<int64> image_jpeg_recompress_quality_for_small_screens_;
  Option<int> image_jpeg_num_progressive_scans_;
  Option<bool> image_retain_color_profile_;
  Option<bool> image_retain_color_sampling_;
  Option<bool> image_retain_exif_data_;

  // Options governing when to retain optimized images vs keep original
  Option<int> image_limit_optimized_percent_;
  Option<int> image_limit_resize_area_percent_;

  // Options related to webp compression.
  Option<int64> image_webp_recompress_quality_;
  Option<int64> image_webp_recompress_quality_for_small_screens_;
  Option<int64> image_webp_timeout_ms_;

  Option<int> image_max_rewrites_at_once_;
  Option<int> max_url_segment_size_;  // For http://a/b/c.d, use strlen("c.d").
  Option<int> max_url_size_;          // This is strlen("http://a/b/c.d").
  // The interval to wait for async rewrites to complete before flushing
  // content.  This deadline is per flush.
  Option<int> rewrite_deadline_ms_;
  // Maximum number of shards for rewritten resources in a directory.
  Option<int> domain_shard_count_;

  Option<EnabledEnum> enabled_;

  Option<bool> distributable_;

  // Encode relevant rewrite options as URL query-parameters so that resources
  // can be reconstructed on servers without the same configuration file.
  Option<bool> add_options_to_urls_;

  // Should in-place-resource-optimization(IPRO) be enabled?
  Option<bool> in_place_rewriting_enabled_;
  // Optimize before responding in in-place flow?
  Option<bool> in_place_wait_for_optimized_;
  // Interval to delay serving on the IPRO path while waiting for optimizations.
  // After this interval, the unoptimized resource will be served.
  Option<int> in_place_rewrite_deadline_ms_;
  // If set, preemptively rewrite images in CSS files on the HTML serving path
  // when IPRO of CSS is enabled.
  Option<bool> in_place_preemptive_rewrite_css_;
  // If set, preemptively rewrite images in CSS files on the IPRO serving path.
  Option<bool> in_place_preemptive_rewrite_css_images_;
  // If set, preemptively rewrite images in image files on the HTML serving path
  // when IPRO of images is enabled.
  Option<bool> in_place_preemptive_rewrite_images_;
  // If set, preemptively rewrite images in JS files on the HTML serving path
  // when IPRO of JS is enabled.
  Option<bool> in_place_preemptive_rewrite_javascript_;
  Option<bool> combine_across_paths_;
  Option<bool> log_rewrite_timing_;   // Should we time HtmlParser?
  Option<bool> lowercase_html_names_;
  Option<bool> always_rewrite_css_;  // For tests/debugging.
  Option<bool> respect_vary_;
  Option<bool> respect_x_forwarded_proto_;
  Option<bool> flush_html_;
  // Should we serve stale responses if the fetch results in a server side
  // error.
  Option<bool> serve_stale_if_fetch_error_;
  // Whether blink critical line flow should be enabled.
  Option<bool> enable_blink_critical_line_;
  // When default_cache_html_ is false (default) we do not cache
  // input HTML which lacks Cache-Control headers. But, when set true,
  // we will cache those inputs for the implicit lifetime just like we
  // do for resources.
  Option<bool> default_cache_html_;
  // In general, we rewrite Cache-Control headers for HTML. We do this
  // for several reasons, but at least one is that our rewrites are not
  // necessarily publicly cacheable.
  // Some people don't like this, so we allow them to disable it.
  Option<bool> modify_caching_headers_;
  // In general, lazyload images loads images on scroll. However, some people
  // may want to load images when the onload event is fired instead. If set to
  // true, images are loaded when onload is fired.
  Option<bool> lazyload_images_after_onload_;
  // The initial image url to load in the lazyload images filter. If this is not
  // specified, we use a 1x1 inlined image.
  Option<GoogleString> lazyload_images_blank_url_;
  // By default, inline_images will inline only critical images. However, some
  // people may want to inline all images (both critical and non-critical). If
  // set to false, all images will be inlined within the html.
  Option<bool> inline_only_critical_images_;
  // Indicates whether image rewriting filters should insert the critical images
  // beacon code.
  Option<bool> critical_images_beacon_enabled_;
  // Indicates whether the DomainRewriteFilter should also do client side
  // rewriting.
  Option<bool> client_domain_rewrite_;
  // Indicates whether the DomainRewriteFilter should rewrite all tags,
  // including <a href> and <form action>.
  Option<bool> domain_rewrite_hyperlinks_;

  // Furious is the A/B experiment framework that uses cookies
  // and Google Analytics to track page speed statistics with
  // multiple sets of rewriters.
  Option<bool> running_furious_;

  Option<int> furious_ga_slot_;

  // Increase the percentage of hits to 10% (current max) that have
  // site speed tracking in Google Analytics.
  Option<bool> increase_speed_tracking_;

  // If enabled we will report time taken before navigating to a new page. This
  // won't have effect, if onload beacon is sent before unload event is
  // trigggered.
  Option<bool> report_unload_time_;

  // Flush more resources if origin is slow to respond.
  Option<bool> flush_more_resources_early_if_time_permits_;

  // Flush more resources in IE and Firefox.
  Option<bool> flush_more_resources_in_ie_and_firefox_;

  // Enables experimental code in defer js.
  Option<bool> enable_defer_js_experimental_;

  // Enables experimental code in inline preview images.
  Option<bool> enable_inline_preview_images_experimental_;

  // Enables the code to lazy load high res images.
  Option<bool> lazyload_highres_images_;

  // Some introspective javascript is very brittle and may break if we
  // make any changes.  Enables code to detect such cases and avoid renaming.
  Option<bool> avoid_renaming_introspective_javascript_;

  // Overrides the IE document mode to use the highest mode available.
  Option<bool> override_ie_document_mode_;

  // Test-only flag to get fetch deadlines to trigger instantly.
  Option<bool> test_instant_fetch_rewrite_deadline_;

  // Enables blocking rewrite of html. RewriteDriver provides a flag
  // fully_rewrite_on_flush which makes sure that all rewrites are done before
  // the response is flushed to the client. If the value of the
  // X-PSA-Blocking-Rewrite header matches this key, the
  // RewriteDriver::fully_rewrite_on_flush flag will be set.
  Option<GoogleString> blocking_rewrite_key_;

  // Number of first N images for which low res image is generated. Negative
  // values will bypass image index check.
  Option<int> max_inlined_preview_images_index_;
  // Minimum image size above which low res image is generated.
  Option<int64> min_image_size_low_resolution_bytes_;
  // Maximum image size below which low res image is generated.
  Option<int64> max_image_size_low_resolution_bytes_;

  // For proxies operating in in-place mode this allows fetching optimized
  // resources from sites that have MPS, etc configured.
  Option<bool> oblivious_pagespeed_urls_;

  // Cache expiration time in msec for properties of finders.
  Option<int64> finder_properties_cache_expiration_time_ms_;

  // Cache refresh time in msec for properties of finders. The properties are
  // refreshed when their age is larger than the specified value. However, the
  // property will be used until finder_properties_cache_expiration_time_ms_.
  Option<int64> finder_properties_cache_refresh_time_ms_;
  // Duration after which the furious experiment cookie will expire on the
  // user's browser (in msec).
  Option<int64> furious_cookie_duration_ms_;

  // The maximum time beyond expiry for which a metadata cache entry may be
  // used.
  Option<int64> metadata_cache_staleness_threshold_ms_;

  // The metadata cache ttl for input resources which are 4xx errors.
  Option<int64> metadata_input_errors_cache_ttl_ms_;

  // The number of milliseconds of cache TTL we assign to resources that
  // are "likely cacheable" (e.g. images, js, css, not html) and have no
  // explicit cache ttl or expiration date.
  Option<int64> implicit_cache_ttl_ms_;

  // Maximum length (in bytes) of response content.
  Option<int64> max_cacheable_response_content_length_;

  // Option for the prioritize_visible_content filter.
  //
  // Represents the following information:
  // "<url family wildcard 1>;<cache time 1>;<comma separated list of non-cacheable elements 1>",
  // "<url family wildcard 2>;<cache time 2>;<comma separated list of non-cacheable elements 2>",
  //  ...
  std::vector<PrioritizeVisibleContentFamily*>
      prioritize_visible_content_families_;

  Option<bool> is_blink_auto_blacklisted_;

  Option<GoogleString> ga_id_;

  // Pass-through request in prioritize_visible_content filter, if we got a
  // non-200 response from origin on the last fetch.
  Option<bool> passthrough_blink_for_last_invalid_response_code_;
  // Sets limit for max html size that is rewritten in Blink.
  Option<int64> blink_max_html_size_rewritable_;
  // Time after which we should try to detect if publisher html in blink
  // has changed.
  Option<int64> blink_html_change_detection_time_ms_;
  // If prioritize_visible_content_families_ is empty and the following is true,
  // then prioritize_visible_content applies on all URLs (with default cache
  // time and no non-cacheables).
  Option<bool> apply_blink_if_no_families_;
  // Show the blink debug dashboard.
  Option<bool> enable_blink_debug_dashboard_;
  // Enable automatic detection of publisher changes in html in blink.
  Option<bool> enable_blink_html_change_detection_;
  // Enable logging of publisher changes detected in html in blink flow.
  Option<bool> enable_blink_html_change_detection_logging_;
  // Use smart diff to detect publisher changes in html in blink.
  Option<bool> use_smart_diff_in_blink_;
  // Don't force disable lazyload in blink;
  Option<bool> enable_lazyload_in_blink_;
  // Enable Prioritizing of scripts in defer javascript.
  Option<bool> enable_prioritizing_scripts_;
  // Enables rewriting of uncacheable resources.
  Option<bool> rewrite_uncacheable_resources_;
  // Override cache-time for cacheable resources in blink.
  Option<int64> override_blink_cache_time_ms_;
  // Non-cacheables to be used for all families in
  // prioritize_visible_content_families_.
  Option<GoogleString> blink_non_cacheables_for_all_families_;
  // Specification for critical line.
  Option<GoogleString> critical_line_config_;
  // A comma delimited list of hosts that can be used to rewrite resources.
  Option<GoogleString> distributed_rewrite_servers_;
  // Time to wait for a distributed rewrite to complete before giving up on the
  // request.
  Option<int64> distributed_rewrite_timeout_ms_;
  // Forbid turning on of any disabled (not enabled) filters either via query
  // parameters or request headers or .htaccess for Directory. Note that this
  // is a latch so that setting it at some directory level forces it on for
  // that and all lower levels, as otherwise someone could just create a
  // sub-directory and enable it in a .htaccess in there.
  Option<bool> forbid_all_disabled_filters_;
  // Enables aggressive rewriters for mobile user agents.
  Option<bool> enable_aggressive_rewriters_for_mobile_;

  // If this is true (it defaults to false) ProxyInterface frontend will
  // reject requests where PSA is not enabled or URL is blacklisted with
  // status code reject_blacklisted_status_code_ (default 403) rather than
  // proxy them in passthrough mode. This does not affect behavior for
  // resource rewriting.
  Option<bool> reject_blacklisted_;
  Option<int> reject_blacklisted_status_code_;

  // Support handling of clients without javascript support.  This is applicable
  // only if any filter that inserts new javascript (e.g., lazyload_images) is
  // enabled.
  Option<bool> support_noscript_enabled_;

  // Maximum size allowed for the combined js resource.
  // Negative value will bypass the size check.
  Option<int64> max_combined_js_bytes_;

  // Url to which pre connect requests will be sent.
  Option<GoogleString> pre_connect_url_;
  // The number of requests for which the status code should remain same so that
  // we consider it to be stable.
  Option<int> property_cache_http_status_stability_threshold_;
  // The maximum number of rewrite info logs stored for a single request.
  Option<int> max_rewrite_info_log_size_;

  // The cache TTL with which to override the urls matching the
  // override_caching_ WildCardGroup. Note that we do not override the cache TTL
  // for any urls if this value is negative. The same TTL value is used for all
  // urls that match override_caching_wildcard_.
  Option<int64> override_caching_ttl_ms_;
  FastWildcardGroup override_caching_wildcard_;

  // Whether to allow logging urls as part of LogRecord.
  Option<bool> allow_logging_urls_in_log_record_;

  // Be sure to update constructor if when new fields is added so that they
  // are added to all_options_, which is used for Merge, and eventually,
  // Compare.
  OptionBaseVector all_options_;
  size_t initialized_options_;  // Counts number of options initialized so far.

  // Array of option names indexed by corresponding OptionEnum.
  static const char* option_enum_to_name_array_[kEndOfOptions];

  // Reverse map from filter id string to corresponding Filter enum.  Note
  // that this is not indexed by filter enum; it's indexed alphabetically by id.
  static const FilterEnumToIdAndNameEntry* filter_id_to_enum_array_[
      kEndOfFilters];

  // Reverse map from option id string to corresponding PropertyBase,
  // from which it is possible to find the 2-4 letter id.  Note that
  // this is not indexed by option enum; it's indexed alphabetically
  // by id.
  static const PropertyBase** option_id_to_property_array_;

  // When compiled for debug, we lazily check whether the all the Option<>
  // member variables in all_options have unique IDs.
  //
  // Note that we include this member-variable in the structrue even under
  // optimization as otherwise it might be very bad news indeed if someone
  // mixed debug/opt object files in an executable.
  bool options_uniqueness_checked_;

  // Do we need to store experiment data for Furious?
  bool need_to_store_experiment_data_;
  // Which experiment configuration are we in?
  int furious_id_;
  int furious_percent_;         // Total traffic going through experiments.
  std::vector<FuriousSpec*> furious_specs_;

  // Headers to add to subresource requests.
  std::vector<NameValue*> custom_fetch_headers_;

  // If this is non-NULL it tells us additional attributes that should be
  // interpreted as containing urls.
  scoped_ptr<std::vector<ElementAttributeCategory> > url_valued_attributes_;

  JavascriptLibraryIdentification javascript_library_identification_;

  DomainLawyer domain_lawyer_;
  FileLoadPolicy file_load_policy_;

  FastWildcardGroup allow_resources_;
  FastWildcardGroup retain_comments_;
  FastWildcardGroup lazyload_enabled_classes_;

  // Using StringPiece here is safe since all entries in this map have static
  // strings as the key.
  typedef std::map<StringPiece, FastWildcardGroup*> FastWildcardGroupMap;
  FastWildcardGroupMap rejected_request_map_;

  GoogleString signature_;

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
