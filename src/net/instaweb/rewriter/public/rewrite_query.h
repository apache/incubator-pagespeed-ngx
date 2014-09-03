// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_

#include "net/instaweb/rewriter/public/device_properties.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/headers.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class GoogleUrl;
class MessageHandler;
class RequestProperties;
class ResponseHeaders;
class RewriteDriver;
class RewriteDriverFactory;
class RewriteFilter;
class RewriteOptions;
class ServerContext;

class RewriteQuery {
 public:
  // The names of query-params.
  static const char kModPagespeed[];
  static const char kPageSpeed[];
  static const char kModPagespeedFilters[];
  static const char kPageSpeedFilters[];
  // ModPagespeed query-param value for redirect from clients that do not
  // support javascript.
  // * Disables all filters that insert new javascript.
  // * Enables filter kHandleNoscriptRedirect.
  static const char kNoscriptValue[];

  enum Status {
    kSuccess,
    kInvalid,
    kNoneFound
  };

  RewriteQuery();
  ~RewriteQuery();

  // Scans request_url's query parameters and request_headers for "ModPagespeed"
  // and "PageSpeed" flags, creating and populating options_ or request_context
  // if any were found that were all parsed successfully. If any were parsed
  // unsuccessfully, kInvalid is returned. If none were found, kNoneFound is
  // returned. Also removes the options from the query_params of the url and the
  // request_headers, populates pagespeed_query_params() with the removed query
  // parameters, and populates pagespeed_option_cookies() with any PageSpeed
  // option cookies in the request headers (which are NOT removed).
  //
  // First cookies are processed, then query parameters, then request headers,
  // then response headers. Therefore parameters set by response headers take
  // precedence over request headers over query parameters over cookies. The
  // exception is filter disables, which always take precedence over enables,
  // even those processed later.
  //
  // If NULL is passed for request_headers or response_headers those particular
  // headers will be skipped in the scan.
  //
  // 'allow_related_options' applies only to .pagespeed. resources.
  // It enables the parsing of filters & options by ID, that have been
  // declared in the RelatedOptions() and RelatedFilters() methods of
  // the filter identified in the .pagespeed. URL.  See GenerateResourceOption
  // for how they get into URLs in the first place.
  //
  // 'allow_options_to_be_specified_by_cookies' controls whether we parse
  // cookies for options.
  Status Scan(bool allow_related_options,
              bool allow_options_to_be_specified_by_cookies,
              const GoogleString& request_option_override,
              const RequestContextPtr& request_context,
              RewriteDriverFactory* factory,
              ServerContext* server_context,
              GoogleUrl* request_url,
              RequestHeaders* request_headers,
              ResponseHeaders* response_headers,
              MessageHandler* handler);

  // Performs the request and response header scanning for Scan(). If any
  // "ModPagespeed" or "PageSpeed" options are found in the headers they are
  // stripped.  Returns kNoneFound if no options found.  Returns kSuccess and
  // populates *'options' if options are found.  Returns kInvalid if any headers
  // were parsed unsuccessfully.  Note: mod_instaweb::build_context_for_request
  // assumes that headers will be stripped from the headers if options are found
  // and that headers will not grow in this call.
  template <class HeaderT>
  static Status ScanHeader(bool allow_options,
                           const GoogleString& request_option_override,
                           const RequestContextPtr& request_context,
                           HeaderT* headers,
                           RequestProperties* request_properties,
                           RewriteOptions* options,
                           MessageHandler* handler);


  // Given a two-letter filter ID string, generates a query-param for
  // any in the driver's options that are related to the filter, and
  // differ from the default.  If no settings have been altered the
  // empty string is returned.
  static GoogleString GenerateResourceOption(StringPiece filter_id,
                                             RewriteDriver* driver);

  // Indicates whether the specified name is likely to identify a
  // custom header or query param.
  static bool MightBeCustomOption(StringPiece name);

  const QueryParams& query_params() const { return query_params_; }
  const QueryParams& pagespeed_query_params() const {
    return pagespeed_query_params_;
  }
  const QueryParams& pagespeed_option_cookies() const {
    return pagespeed_option_cookies_;
  }
  const RewriteOptions* options() const { return options_.get(); }
  RewriteOptions* ReleaseOptions() { return options_.release(); }

  // Determines whether the status code is one that is acceptable for
  // processing requests.
  static bool IsOK(Status status) {
    return (status == kNoneFound) || (status == kSuccess);
  }

 private:
  friend class RewriteQueryTest;
  FRIEND_TEST(RewriteQueryTest, ClientOptionsEmptyHeader);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsMultipleHeaders);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsOrder1);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsOrder2);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsCaseInsensitive);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsNonDefaultProxyMode);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsValidVersionBadOptions);
  FRIEND_TEST(RewriteQueryTest, ClientOptionsInvalidVersion);

  enum ProxyMode {
    // Client prefers that the server operates in its default mode.
    kProxyModeDefault,
    // Client prefers that no image be transformed.
    kProxyModeNoImageTransform,
    // Client prefers that no resource be transformed.
    // This is equivalent to "?PageSpeedFilters=" in the request URL.
    kProxyModeNoTransform,
  };

  // Returns true if the params/headers/cookies look like they might have
  // some options.  This is used as a cheap pre-scan before doing the more
  // expensive query processing.
  static bool MayHaveCustomOptions(
      const QueryParams& params, const RequestHeaders* req_headers,
      const ResponseHeaders* resp_headers,
      const RequestHeaders::CookieMultimap& cookies);

  // As above, but only for headers.
  template <class HeaderT>
  static bool HeadersMayHaveCustomOptions(const QueryParams& params,
                                          const HeaderT* headers);

  // As above, but only for cookies.
  static bool CookiesMayHaveCustomOptions(
      const RequestHeaders::CookieMultimap& cookies);

  // Examines a name/value pair for options.
  static Status ScanNameValue(const StringPiece& name,
                              const StringPiece& value,
                              bool allow_options,
                              const RequestContextPtr& request_context,
                              RequestProperties* request_properties,
                              RewriteOptions* options,
                              MessageHandler* handler);

  // Parses a resource option based on the specified filter's related options.
  static Status ParseResourceOption(StringPiece value, RewriteOptions* options,
                                    const RewriteFilter* rewrite_filter);

  // Returns true if a kXPsaClientOptions header is found, parsed successfully,
  // and valid proxy_mode and image_quality are returned.
  static bool ParseClientOptions(
      const StringPiece& client_options,
      ProxyMode* proxy_mode,
      DeviceProperties::ImageQualityPreference* image_quality);

  // Set image qualities in options.
  // Returns true if any option is explicitly set.
  static bool SetEffectiveImageQualities(
      DeviceProperties::ImageQualityPreference quality_preference,
      RequestProperties* request_properties,
      RewriteOptions* options);

  // Returns true if any option is explicitly set.
  static bool UpdateRewriteOptionsWithClientOptions(
      StringPiece header_value, RequestProperties* request_properties,
      RewriteOptions* options);

  // Returns true if a valid ProxyMode parsed and returned.
  static bool ParseProxyMode(const GoogleString* mode_name, ProxyMode* mode);

  // Returns true if a valid ImageQualityPreference parsed and returned.
  static bool ParseImageQualityPreference(
      const GoogleString* preference_name,
      DeviceProperties::ImageQualityPreference* preference);

  QueryParams query_params_;
  QueryParams pagespeed_query_params_;
  QueryParams pagespeed_option_cookies_;
  scoped_ptr<RewriteOptions> options_;

  DISALLOW_COPY_AND_ASSIGN(RewriteQuery);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_
