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

#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class MessageHandler;
class QueryParams;
class RequestHeaders;
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
  static const char kModPagespeedFilters[];
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

  // Scans request_url's query parameters and request_headers for "ModPagespeed"
  // flags, creating and populating *'options' if any were found they were all
  // parsed successfully.  If any were parsed unsuccessfully kInvalid is
  // returned.  If none found, kNoneFound is returned. It also removes the
  // "ModPagespeed" flags from the query_params of the url and the
  // request_headers.
  //
  // First queries are processed, then request headers, then response headers.
  // Therefore parameters set by response headers take precedence over request
  // headers over query parameters. The exception is filter disables, which
  // always take precedence over enables, even those processed later.
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
  // TODO(jmarantz): consider allowing an alternative prefix to "ModPagespeed"
  // to accomodate other Page Speed Automatic applications that might want to
  // brand differently.
  static Status Scan(bool allow_related_options,
                     RewriteDriverFactory* factory,
                     ServerContext* server_context,
                     GoogleUrl* request_url,
                     RequestHeaders* request_headers,
                     ResponseHeaders* response_headers,
                     scoped_ptr<RewriteOptions>* options,
                     MessageHandler* handler);

  // Performs the request and response header scanning for Scan(). If any
  // "ModPagespeed" options are found in the headers they are stripped.
  // Returns kNoneFound if no options found.  Returns kSuccess and
  // populates *'options' if options are found.  Returns kInvalid if
  // any headers were parsed unsuccessfully.
  // Note: mod_instaweb::build_context_for_request assumes that headers will be
  // stripped from the headers if options are found and that headers will not
  // grow in this call.
  template <class HeaderT>
  static Status ScanHeader(HeaderT* headers,
                           RewriteOptions* options,
                           MessageHandler* handler);


  // Given a two-letter filter ID string, generates a query-param for
  // any in the driver's options that are related to the filter, and
  // differ from the default.  If no settings have been altered the
  // empty string is returned.
  static GoogleString GenerateResourceOption(StringPiece filter_id,
                                             RewriteDriver* driver);

 private:
  // Returns true if the params/headers look like they might have some
  // options.  This is used as a cheap pre-scan before doing the more
  // expensive query processing.
  static bool MayHaveCustomOptions(const QueryParams& params,
                                   const RequestHeaders* req_headers,
                                   const ResponseHeaders* resp_headers);

  // As above, but only for headers.
  template <class HeaderT>
  static bool HeadersMayHaveCustomOptions(const QueryParams& params,
                                          const HeaderT* headers);

  // Examines a name/value pair for options.
  static Status ScanNameValue(const StringPiece& name,
                              const GoogleString& value,
                              RewriteOptions* options,
                              MessageHandler* handler);

  // Parses a resource option based on the specified filter's related options.
  static Status ParseResourceOption(StringPiece value, RewriteOptions* options,
                                    const RewriteFilter* rewrite_filter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_
