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

#include "net/instaweb/rewriter/public/rewrite_query.h"

#include <algorithm>  // for std::binary_search
#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace {

// We use + and = inside the resource-options URL segment because they will not
// be quoted by UrlEscaper, unlike "," and ":".
const char kResourceFilterSeparator[] = "+";
const char kResourceOptionValueSeparator[] = "=";

const char kProxyOptionSeparator[] = ",";
const char kProxyOptionValueSeparator = '=';
const char kProxyOptionVersion[] = "v";
const char kProxyOptionMode[] = "m";
const char kProxyOptionImageQualityPreference[] = "iqp";
const char kProxyOptionValidVersionValue[] = "1";

StringPiece SanitizeValueAsQP(StringPiece untrusted_value,
                              GoogleUrl* storage) {
  // This is ever so slightly hacky: we dummy up an URL with a QP where the
  // value of the QP is the untrusted value, then we discard everything
  // prior to the possibly-modified value in the resulting GoogleUrl.
  const char kUrlBase[] = "http://www.example.com/?x=";
  storage->Reset(StrCat(kUrlBase, untrusted_value));
  StringPiece sanitized_value = storage->Spec();
  return StringPiece(sanitized_value.data() + STATIC_STRLEN(kUrlBase),
                     sanitized_value.size() - STATIC_STRLEN(kUrlBase));
}

}  // namespace

const char RewriteQuery::kModPagespeed[] = "ModPagespeed";
const char RewriteQuery::kPageSpeed[] = "PageSpeed";

const char RewriteQuery::kModPagespeedFilters[] = "ModPagespeedFilters";
const char RewriteQuery::kPageSpeedFilters[] = "PageSpeedFilters";

const char RewriteQuery::kNoscriptValue[] = "noscript";

template <class HeaderT>
RewriteQuery::Status RewriteQuery::ScanHeader(
    bool allow_options,
    const GoogleString& request_option_override,
    const RequestContextPtr& request_context,
    HeaderT* headers,
    RequestProperties* request_properties,
    RewriteOptions* options,
    MessageHandler* handler) {
  Status status = kNoneFound;

  if (headers == NULL) {
    return status;
  }

  // Check to see if the override token exists.
  if (!allow_options && !request_option_override.empty()) {
    GoogleString mod_pagespeed_override =
        StrCat(kModPagespeed, RewriteOptions::kRequestOptionOverride);
    GoogleString page_speed_override =
        StrCat(kPageSpeed, RewriteOptions::kRequestOptionOverride);
    for (int i = 0, n = headers->NumAttributes(); i < n; ++i) {
      const StringPiece name(headers->Name(i));
      const GoogleString& value = headers->Value(i);
      if (name == mod_pagespeed_override || name == page_speed_override) {
        allow_options = (value == request_option_override);
        break;
      }
    }
  }

  // Tracks the headers that need to be removed.
  // It doesn't matter what type of headers we use, so we use RequestHeaders.
  RequestHeaders headers_to_remove;

  for (int i = 0, n = headers->NumAttributes(); i < n; ++i) {
    const StringPiece name(headers->Name(i));
    const GoogleString& value = headers->Value(i);
    switch (ScanNameValue(name, value, allow_options, request_context,
                          request_properties, options, handler)) {
      case kNoneFound:
        break;
      case kSuccess:
        if (name.starts_with(kModPagespeed) || name.starts_with(kPageSpeed)) {
          headers_to_remove.Add(name, value);
        }
        status = kSuccess;
        break;
      case kInvalid:
        return kInvalid;
    }
  }

  // TODO(bolian): jmarantz suggested below change.  we should make a
  // StringSetInsensitive and put all the names we want to remove including
  // XPSAClientOptions and then call RemoveAllFromSet.
  // That will be more efficient.
  for (int i = 0, n = headers_to_remove.NumAttributes(); i < n; ++i) {
    headers->Remove(headers_to_remove.Name(i), headers_to_remove.Value(i));
  }
  // kXPsaClientOptions is meant for proxy only. Remove it in any case.
  headers->RemoveAll(HttpAttributes::kXPsaClientOptions);

  return status;
}

RewriteQuery::RewriteQuery() {
}

RewriteQuery::~RewriteQuery() {
}

// Scan for option-sets in cookies, query params, request and response headers.
// We only allow a limited number of options to be set. In particular, some
// options are risky to set this way, such as image inline threshold, which
// exposes a DOS vulnerability and a risk of poisoning our internal cache, and
// domain adjustments, which can introduce a security vulnerability.
RewriteQuery::Status RewriteQuery::Scan(
    bool allow_related_options,
    bool allow_options_to_be_specified_by_cookies,
    const GoogleString& request_option_override,
    const RequestContextPtr& request_context,
    RewriteDriverFactory* factory,
    ServerContext* server_context,
    GoogleUrl* request_url,
    RequestHeaders* request_headers,
    ResponseHeaders* response_headers,
    MessageHandler* handler) {
  Status status = kNoneFound;
  query_params_.Clear();
  pagespeed_query_params_.Clear();
  pagespeed_option_cookies_.Clear();
  options_.reset(NULL);

  // To support serving resources from servers that don't share the
  // same settings as the ones generating HTML, we can put whitelisted
  // option-settings into the query-params by ID.  But we expose this
  // setting (a) only for .pagespeed. resources, not HTML, and (b)
  // only when allow_related_options is true.
  ResourceNamer namer;

  bool return_after_parsing = false;
  if (allow_related_options &&
      namer.DecodeIgnoreHashAndSignature(request_url->LeafSansQuery()) &&
      namer.has_options()) {
    const RewriteFilter* rewrite_filter =
        server_context->FindFilterForDecoding(namer.id());
    if (rewrite_filter != NULL) {
      options_.reset(factory->NewRewriteOptionsForQuery());
      status = ParseResourceOption(namer.options(), options_.get(),
                                   rewrite_filter);
      if (status != kSuccess) {
        options_.reset(NULL);

        // We want query_params() to be populated after calling
        // RewriteQuery::Scan, even if any URL-embedded configuration
        // parameters are invalid.  So we delay our early exit until
        // after the query_params_.Parse call below.
        return_after_parsing = true;
      }
    }
  }

  // Extract all cookies iff we can use them to set options.
  RequestHeaders::CookieMultimap no_cookies;
  const RequestHeaders::CookieMultimap& all_cookies(
      (allow_options_to_be_specified_by_cookies && request_headers != NULL)
      ? request_headers->GetAllCookies()
      : no_cookies);

  // See if anything looks even remotely like one of our options before doing
  // any more work.  Note that when options are correctly embedded in the URL,
  // we will have a success-status here.  But we still allow a hand-added
  // query-param to override the embedded options.
  query_params_.ParseFromUrl(*request_url);
  if (return_after_parsing ||
      !MayHaveCustomOptions(query_params_, request_headers, response_headers,
                            all_cookies)) {
    return status;
  }

  if (options_.get() == NULL) {
    options_.reset(factory->NewRewriteOptionsForQuery());
  }

  scoped_ptr<RequestProperties> request_properties;
  if (request_headers != NULL) {
    request_properties.reset(server_context->NewRequestProperties());
    request_properties->SetUserAgent(
        request_headers->Lookup1(HttpAttributes::kUserAgent));
  }

  // Check to see if options should be parsed.
  // If the config disallows parsing, and the proper token is not provided,
  // do not use the options passed in the url.
  bool allow_options = true;
  if (!request_option_override.empty()) {
    allow_options = false;
    GoogleString override_token;
    GoogleString mod_pagespeed_override =
        StrCat(kModPagespeed, RewriteOptions::kRequestOptionOverride);
    GoogleString page_speed_override =
        StrCat(kPageSpeed, RewriteOptions::kRequestOptionOverride);
    if (query_params_.Lookup1Unescaped(mod_pagespeed_override,
                                       &override_token) ||
        query_params_.Lookup1Unescaped(page_speed_override, &override_token)) {
      allow_options = (override_token == request_option_override);
    }
  }

  // Scan for options set as cookies. They can be overridden by QPs or headers.
  // An explanation of the life cycle of a PageSpeed option cookie:
  // * Initially the value is passed in as a GoogleUrl query parameter.
  // * GoogleUrl does minimal escaping, mainly removing whitespace and
  //   percent-encoding control characters.
  // * That is parsed by QueryParams (above), which does no further escaping.
  // * Since cookie values have restrictions on the characters allowed in the
  //   value (e.g. no ';'s), we GoogleUrl::Escape the value, which does
  //   significant percent-escaping (nearly everything except alphanumeric).
  //   This is done in ResponseHeaders::SetPageSpeedQueryParamsAsCookies().
  // [So now we're here, where we use the cookie values set by the above steps]
  // * We GoogleUrl::Unescape the cookie value to reverse the previous step.
  //   Note that GoogleUrl::Unescape(GoogleUrl::Escape(x)) is the identify
  //   function, so we expect the value to be GoogleUrl minimally escaped.
  // * We sanitize the unescaped cookie value by dummying up a GoogleUrl with
  //   the value as a query parameter value, hence re-minimally escaping it.
  // * We then escape this sanitized value since that's the process that we
  //   went through above: if this escaped value equals the cookie value then
  //   the cookie value seems authentic -and- the unescaped value must also be
  //   sanitized, meaning it's safe to pass to our value parsing logic. If the
  //   escaped value does -not- equal the cookie value, it means the cookie has
  //   characters that we cannot have put there, and we assume that someone has
  //   manually set it in an attempt to circumvent our precautions, so we
  //   ignore the cookie completely.
  RequestHeaders::CookieMultimapConstIter it, end;
  for (it = all_cookies.begin(), end = all_cookies.end(); it != end; ++it) {
    StringPiece cookie_name = it->first;
    if (MightBeCustomOption(cookie_name)) {
      GoogleUrl gurl;
      StringPiece cookie_value = it->second.first;
      GoogleString unescaped = GoogleUrl::UnescapeIgnorePlus(cookie_value);
      StringPiece sanitized = SanitizeValueAsQP(unescaped, &gurl);
      GoogleString escaped = GoogleUrl::Escape(sanitized);
      if (unescaped == sanitized && escaped == cookie_value) {
        RequestContextPtr null_request_context;
        if (ScanNameValue(cookie_name, unescaped, allow_options,
                          null_request_context, request_properties.get(),
                          options_.get(), handler) == kSuccess) {
          pagespeed_option_cookies_.AddEscaped(cookie_name, unescaped);
          status = kSuccess;
        }
      } else {
        // PageSpeed cookies with an invalid value will not be cleared. This
        // is unfortunate but OK since they'll never apply (due to the invalid
        // value) and will eventually expire anyway.
        handler->Message(kInfo, "PageSpeed Cookie value seems mangled: "
                         "name='%s', value='%s', escaped value='%s'",
                         cookie_name.as_string().c_str(),
                         cookie_value.as_string().c_str(),
                         escaped.c_str());
      }
    }
  }

  pagespeed_query_params_.Clear();
  QueryParams temp_query_params;
  for (int i = 0; i < query_params_.size(); ++i) {
    GoogleString unescaped_value;
    if (query_params_.UnescapedValue(i, &unescaped_value)) {
      // The Unescaper changes "+" to " ", which is not what we want, and
      // is not what happens for response headers and request headers, so
      // let's fix it now.
      GlobalReplaceSubstring(" " , "+", &unescaped_value);
      switch (ScanNameValue(
          query_params_.name(i), unescaped_value, allow_options,
          request_context, request_properties.get(), options_.get(), handler)) {
        case kNoneFound:
          // If this is not a PageSpeed-related query-parameter, then save it
          // in its escaped form.
          temp_query_params.AddEscaped(query_params_.name(i),
                                       *query_params_.EscapedValue(i));
          break;
        case kSuccess:
          // If it is a PageSpeed-related query parameter, also save it so we
          // can add it back if we receive a redirection response to our fetch.
          pagespeed_query_params_.AddEscaped(query_params_.name(i),
                                            *query_params_.EscapedValue(i));
          status = kSuccess;
          break;
        case kInvalid:
          status = kInvalid;
          options_.reset(NULL);
          return status;
      }
    } else {
      temp_query_params.AddEscaped(query_params_.name(i), NULL);
    }
  }
  if (status == kSuccess) {
    // Remove the ModPagespeed* or PageSpeed* for url.
    GoogleString temp_params = temp_query_params.empty() ? "" :
        StrCat("?", temp_query_params.ToEscapedString());
    request_url->Reset(StrCat(request_url->AllExceptQuery(), temp_params,
                              request_url->AllAfterQuery()));
  }

  switch (ScanHeader<RequestHeaders>(
      allow_options, request_option_override, request_context, request_headers,
      request_properties.get(), options_.get(), handler)) {
    case kNoneFound:
      break;
    case kSuccess:
      status = kSuccess;
      break;
    case kInvalid:
      status = kInvalid;
      options_.reset(NULL);
      return status;
  }

  switch (ScanHeader<ResponseHeaders>(
      allow_options, request_option_override, request_context, response_headers,
      request_properties.get(), options_.get(), handler)) {
    case kNoneFound:
      break;
    case kSuccess:
      status = kSuccess;
      break;
    case kInvalid:
      status = kInvalid;
      options_.reset(NULL);
      return status;
  }

  // Set a default rewrite level in case the mod_pagespeed server has no
  // rewriting options configured.
  // Note that if any filters are explicitly set with
  // PageSpeedFilters=..., then the call to
  // DisableAllFiltersNotExplicitlyEnabled() below will make the 'level'
  // irrelevant.
  switch (status) {
    case kSuccess:
      options_->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
      break;
    case kNoneFound:
      options_.reset(NULL);
      break;
    case kInvalid:
      LOG(DFATAL) << "Invalid responses always use early exit";
      options_.reset(NULL);
      break;
  }
  return status;
}

bool RewriteQuery::MightBeCustomOption(StringPiece name) {
  // TODO(jmarantz): switch to case-insenstive comparisons for these prefixes.
  return name.starts_with(kModPagespeed) || name.starts_with(kPageSpeed) ||
      StringCaseEqual(name, HttpAttributes::kXPsaClientOptions);
}

template <class HeaderT>
bool RewriteQuery::HeadersMayHaveCustomOptions(const QueryParams& params,
                                               const HeaderT* headers) {
  if (headers != NULL) {
    for (int i = 0, n = headers->NumAttributes(); i < n; ++i) {
      if (MightBeCustomOption(headers->Name(i))) {
        return true;
      }
    }
  }
  return false;
}

bool RewriteQuery::CookiesMayHaveCustomOptions(
    const RequestHeaders::CookieMultimap& cookies) {
  RequestHeaders::CookieMultimapConstIter it = cookies.begin();
  RequestHeaders::CookieMultimapConstIter end = cookies.end();
  for (; it != end; ++it) {
    if (MightBeCustomOption(it->first)) {
      return true;
    }
  }
  return false;
}

bool RewriteQuery::MayHaveCustomOptions(
    const QueryParams& params, const RequestHeaders* req_headers,
    const ResponseHeaders* resp_headers,
    const RequestHeaders::CookieMultimap& cookies) {
  for (int i = 0, n = params.size(); i < n; ++i) {
    if (MightBeCustomOption(params.name(i))) {
      return true;
    }
  }
  if (HeadersMayHaveCustomOptions(params, req_headers)) {
    return true;
  }
  if (HeadersMayHaveCustomOptions(params, resp_headers)) {
    return true;
  }
  if (CookiesMayHaveCustomOptions(cookies)) {
    return true;
  }
  if (req_headers != NULL &&
      (req_headers->Has(HttpAttributes::kXPsaClientOptions) ||
       req_headers->HasValue(HttpAttributes::kCacheControl, "no-transform"))) {
    return true;
  }
  if ((resp_headers != NULL) && resp_headers->HasValue(
          HttpAttributes::kCacheControl, "no-transform")) {
    return true;
  }
  return false;
}

RewriteQuery::Status RewriteQuery::ScanNameValue(
    const StringPiece& name, const StringPiece& value, bool allow_options,
    const RequestContextPtr& request_context,
    RequestProperties* request_properties, RewriteOptions* options,
    MessageHandler* handler) {
  Status status = kNoneFound;

  // See https://code.google.com/p/modpagespeed/issues/detail?id=627
  // Evidently bots and other clients may not properly resolve the quoted
  // URLs we send into noscript links, so remove any excess quoting we
  // see around the value.
  StringPiece trimmed_value(value);
  TrimUrlQuotes(&trimmed_value);
  if (name == kModPagespeed || name == kPageSpeed) {
    RewriteOptions::EnabledEnum enabled;
    if (RewriteOptions::ParseFromString(trimmed_value, &enabled)) {
      options->set_enabled(enabled);
      status = kSuccess;
    } else if (trimmed_value.starts_with(kNoscriptValue)) {
      // We use starts_with("noscript") to help resolve Issue 874.
      // Disable filters that depend on custom script execution.
      options->DisableFiltersRequiringScriptExecution();
      options->EnableFilter(RewriteOptions::kHandleNoscriptRedirect);
      status = kSuccess;
    } else {
      // TODO(sligocki): Return 404s instead of logging server errors here
      // and below.
      handler->Message(kWarning, "Invalid value for %s: %s "
                       "(should be on, off, unplugged, or noscript)",
                       name.as_string().c_str(),
                       trimmed_value.as_string().c_str());
      status = kInvalid;
    }
  } else if (!allow_options) {
    status = kNoneFound;
  } else if (name == kModPagespeedFilters || name == kPageSpeedFilters) {
    // When using PageSpeedFilters query param, only the specified filters
    // should be enabled.
    if (options->AdjustFiltersByCommaSeparatedList(trimmed_value, handler)) {
      status = kSuccess;
    } else {
      status = kInvalid;
    }
  } else if (StringCaseEqual(name, HttpAttributes::kXPsaClientOptions)) {
    if (UpdateRewriteOptionsWithClientOptions(
        trimmed_value, request_properties, options)) {
      status = kSuccess;
    }
    // We don't want to return kInvalid, which causes 405 (kMethodNotAllowed)
    // returned to client.
  } else if (StringCaseEqual(name, HttpAttributes::kCacheControl)) {
    StringPieceVector pairs;
    SplitStringPieceToVector(trimmed_value, ",", &pairs,
                             true /* omit_empty_strings */);
    for (int i = 0, n = pairs.size(); i < n; ++i) {
      TrimWhitespace(&pairs[i]);
      if (pairs[i] == "no-transform") {
        // TODO(jmarantz): A .pagespeed resource should return un-optimized
        // content with "Cache-Control: no-transform".
        options->set_enabled(RewriteOptions::kEnabledOff);
        status = kSuccess;
        break;
      }
    }
  } else if (name.starts_with(kModPagespeed) || name.starts_with(kPageSpeed)) {
    // Remove the initial ModPagespeed or PageSpeed.
    StringPiece name_suffix = name;
    stringpiece_ssize_type prefix_len;
    if (name.starts_with(kModPagespeed)) {
      prefix_len = sizeof(kModPagespeed)-1;
    } else {
      prefix_len = sizeof(kPageSpeed)-1;
    }
    name_suffix.remove_prefix(prefix_len);
    switch (options->SetOptionFromQuery(name_suffix, trimmed_value)) {
      case RewriteOptions::kOptionOk:
        status = kSuccess;
        break;
      case RewriteOptions::kOptionNameUnknown:
        if (request_context.get() != NULL &&
            StringCaseEqual(name_suffix,
                            RewriteOptions::kStickyQueryParameters)) {
          request_context->set_sticky_query_parameters_token(trimmed_value);
          status = kSuccess;
        } else {
          status = kNoneFound;
        }
        break;
      case RewriteOptions::kOptionValueInvalid:
        status = kInvalid;
        break;
    }
  }
  return status;
}

// In some environments it is desirable to bind a URL to the options
// that affect it.  One example of where this would be needed is if
// images are served by a separate cluster that doesn't share the same
// configuration as the mod_pagespeed instances that rewrote the HTML.
// In this case, we must encode the relevant options as query-params
// to be appended to the URL.  These should be decodable by Scan()
// above, though they don't need to be in the same verbose format that
// we document for debugging and experimentation.  They can use the
// more concise abbreviations of 2-4 letters for each option.
GoogleString RewriteQuery::GenerateResourceOption(
    StringPiece filter_id, RewriteDriver* driver) {
  const RewriteFilter* filter = driver->FindFilter(filter_id);
  // TODO(sligocki): We do not seem to be detecting Apache crashes in the
  // system_test. We should detect and fail when these crashes occur.
  CHECK(filter != NULL)
      << "Filter ID " << filter_id << " is not registered in RewriteDriver. "
      << "You must register it with a call to RegisterRewriteFilter() in "
      << "RewriteDriver::SetServerContext().";
  StringPiece prefix("");
  GoogleString value;
  const RewriteOptions* options = driver->options();

  // All the filters & options will be encoded into the value of a
  // single query param with name kAddQueryFromOptionName ("PsolOpt").
  // The value will have the comma-separated filters IDs, and option IDs,
  // which are all given a 2-4 letter codes.  The only difference between
  // options & filters syntactically is that options have values preceded
  // by a colon:
  //   filter1,filter2,filter3,option1:value1,option2:value2

  // Add any relevant enabled filters.
  int num_filters;
  const RewriteOptions::Filter* filters = filter->RelatedFilters(&num_filters);
  for (int i = 0; i < num_filters; ++i) {
    RewriteOptions::Filter filter_enum = filters[i];
    if (options->Enabled(filter_enum)) {
      StrAppend(&value, prefix, RewriteOptions::FilterId(filter_enum));
      prefix = kResourceFilterSeparator;
    }
  }

  // Add any non-default options.
  GoogleString option_value;
  const StringPieceVector* opts = filter->RelatedOptions();
  for (int i = 0, n = (opts == NULL ? 0 : opts->size()); i < n; ++i) {
    StringPiece option = (*opts)[i];
    const char* id;
    bool was_set = false;
    if (options->OptionValue(option, &id, &was_set, &option_value) && was_set) {
      StrAppend(&value, prefix, id, kResourceOptionValueSeparator,
                option_value);
      prefix = kResourceFilterSeparator;
    }
  }
  return value;
}

RewriteQuery::Status RewriteQuery::ParseResourceOption(
    StringPiece value, RewriteOptions* options, const RewriteFilter* filter) {
  Status status = kNoneFound;
  StringPieceVector filters_and_options;
  SplitStringPieceToVector(value, kResourceFilterSeparator,
                           &filters_and_options, true);

  // We will want to validate any filters & options we are trying to set
  // with this mechanism against the whitelist of whatever the filter thinks is
  // needed.  But do this lazily.
  int num_filters;
  const RewriteOptions::Filter* filters = filter->RelatedFilters(&num_filters);
  const StringPieceVector* opts = filter->RelatedOptions();

  for (int i = 0, n = filters_and_options.size(); i < n; ++i) {
    StringPieceVector name_value;
    SplitStringPieceToVector(filters_and_options[i],
                             kResourceOptionValueSeparator, &name_value, true);
    switch (name_value.size()) {
      case 1: {
        RewriteOptions::Filter filter_enum =
            RewriteOptions::LookupFilterById(name_value[0]);
        if ((filter_enum == RewriteOptions::kEndOfFilters) ||
            !std::binary_search(filters, filters + num_filters, filter_enum)) {
          status = kInvalid;
        } else {
          options->EnableFilter(filter_enum);
          status = kSuccess;
        }
        break;
      }
      case 2: {
        StringPiece option_name =
            RewriteOptions::LookupOptionNameById(name_value[0]);
        if (!option_name.empty() &&
            opts != NULL &&
            std::binary_search(opts->begin(), opts->end(), option_name) &&
            options->SetOptionFromName(option_name, name_value[1])
            == RewriteOptions::kOptionOk) {
          status = kSuccess;
        } else {
          status = kInvalid;
        }
        break;
      }
      default:
        status = kInvalid;
    }
  }
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->DisableAllFiltersNotExplicitlyEnabled();
  return status;
}

bool RewriteQuery::ParseProxyMode(
    const GoogleString* mode_name, ProxyMode* mode) {
  int mode_value = 0;
  if (mode_name != NULL &&
      !mode_name->empty() &&
      StringToInt(*mode_name, &mode_value) &&
      mode_value >= kProxyModeDefault &&
      mode_value <= kProxyModeNoTransform) {
    *mode = static_cast<ProxyMode>(mode_value);
    return true;
  }
  return false;
}

bool RewriteQuery::ParseImageQualityPreference(
    const GoogleString* preference_value,
    DeviceProperties::ImageQualityPreference* preference) {
  int value = 0;
  if (preference_value != NULL &&
      !preference_value->empty() &&
      StringToInt(*preference_value, &value) &&
      value >= DeviceProperties::kImageQualityDefault &&
      value <= DeviceProperties::kImageQualityHigh) {
    *preference = static_cast<DeviceProperties::ImageQualityPreference>(value);
    return true;
  }
  return false;
}

bool RewriteQuery::ParseClientOptions(
    const StringPiece& client_options, ProxyMode* proxy_mode,
    DeviceProperties::ImageQualityPreference* image_quality_preference) {
  StringMultiMapSensitive options;
  options.AddFromNameValuePairs(
      client_options, kProxyOptionSeparator, kProxyOptionValueSeparator,
      true);

  const GoogleString* version_value = options.Lookup1(kProxyOptionVersion);
  // We only support version value of kProxyOptionValidVersionValue for now.
  // New supported version might be added later.
  if (version_value != NULL &&
      *version_value == kProxyOptionValidVersionValue) {
    *proxy_mode = kProxyModeDefault;
    *image_quality_preference = DeviceProperties::kImageQualityDefault;
    ParseProxyMode(options.Lookup1(kProxyOptionMode), proxy_mode);

    if (*proxy_mode == kProxyModeDefault) {
      ParseImageQualityPreference(
          options.Lookup1(kProxyOptionImageQualityPreference),
          image_quality_preference);
    }
    return true;
  }
  return false;
}

bool RewriteQuery::SetEffectiveImageQualities(
    DeviceProperties::ImageQualityPreference quality_preference,
    RequestProperties* request_properties,
    RewriteOptions* options) {
  if (quality_preference == DeviceProperties::kImageQualityDefault ||
      request_properties == NULL) {
    return false;
  }
  int webp = -1, jpeg = -1;
  if (request_properties->GetPreferredImageQualities(
      quality_preference, &webp, &jpeg)) {
    options->set_image_webp_recompress_quality(webp);
    options->set_image_jpeg_recompress_quality(jpeg);
    return true;
  }
  return false;
}

bool RewriteQuery::UpdateRewriteOptionsWithClientOptions(
    StringPiece client_options, RequestProperties* request_properties,
    RewriteOptions* options) {
  ProxyMode proxy_mode = kProxyModeDefault;
  DeviceProperties::ImageQualityPreference quality_preference =
      DeviceProperties::kImageQualityDefault;
  if (!ParseClientOptions(client_options, &proxy_mode, &quality_preference)) {
    return false;
  }

  if (proxy_mode == kProxyModeNoTransform) {
    options->DisableAllFilters();
    return true;
  } else if (proxy_mode == kProxyModeNoImageTransform) {
    ImageRewriteFilter::DisableRelatedFilters(options);
    return true;
  } else if (proxy_mode == kProxyModeDefault) {
    return SetEffectiveImageQualities(
        quality_preference, request_properties, options);
  }
  DCHECK(false);
  return false;
}

}  // namespace net_instaweb
