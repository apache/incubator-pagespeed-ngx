// Copyright 2010 Google Inc.
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
//
// Author: lsong@google.com (Libo Song)
//         jmarantz@google.com (Joshua Marantz)
//
// The Apache handler for rewritten resources and a couple other Apache hooks.

#ifndef PAGESPEED_APACHE_INSTAWEB_HANDLER_H_
#define PAGESPEED_APACHE_INSTAWEB_HANDLER_H_

#include "pagespeed/apache/apache_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

#include "apr_pools.h"  // for apr_status_t
// The httpd header must be after the instaweb_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "pagespeed/apache/apache_httpd_includes.h"

namespace net_instaweb {

class ApacheConfig;
class ApacheRequestContext;
class ApacheRewriteDriverFactory;
class ApacheServerContext;
class InPlaceResourceRecorder;

// Context for handling a request, computing options and request headers in
// the constructor.
//
// TODO(jmarantz): There are many static methods in this class.  Some
// of them need to stay that way as they are used as C entry points.
// Some are helper methods, and we could adopt a policy of requiring
// the static methods to instantiate the class (possibly making its
// constructor lighter weight) and then calling them explicitly.  For
// the time being, we'll leave these static methods with the
// non-compliant lower_case_names_with_underscores naming convention
// and fix their naming when we update whether they should be static
// or not.
class InstawebHandler {
 public:
  explicit InstawebHandler(request_rec* request);
  ~InstawebHandler();

  // Any PageSpeed query params are removed.
  const GoogleUrl& stripped_gurl() const { return stripped_gurl_; }
  const RequestContextPtr request_context() const { return request_context_; }
  bool use_custom_options() const { return custom_options_.get() != NULL; }
  const QueryParams& query_params() { return rewrite_query_.query_params(); }
  const QueryParams& pagespeed_query_params() {
    return rewrite_query_.pagespeed_query_params();
  }
  const QueryParams& pagespeed_option_cookies() {
    return rewrite_query_.pagespeed_option_cookies();
  }

  void RemoveStrippedResponseHeadersFromApacheRequest();

  // Makes a driver from the request_context and options.  Note that
  // this can only be called once, as it potentially mutates the options
  // as it transfers ownership of custom_options. The driver is owned by
  // the InstawebHandler and will be cleaned up at destruction, unless you
  // call DisownDriver().
  RewriteDriver* MakeDriver();

  // Prevent "this" from cleaning up rewrite_driver_ at destruction.
  void DisownDriver();

  // Allocates a Fetch object associated with the current request and
  // the specified URL.  Include in debug_info anything that's cheap to create
  // and would be informative if something went wrong with the fetch.
  // If any uses will be from other threads you must set buffered=true to keep
  // your other thread from getting blocked if our output is being read by a
  // slow reader.
  ApacheFetch* MakeFetch(
      const GoogleString& url, bool buffered, StringPiece debug_info);

  // Allocates a Fetch object associated with the current request and its URL.
  // Please read the comment above before setting buffered=false.
  ApacheFetch* MakeFetch(bool buffered, StringPiece debug_info) {
    return MakeFetch(original_url_, buffered, debug_info);
  }

  // Attempts to handle this as a proxied resource (see
  // MapProxyDomain). Returns false if the proxy handling didn't
  // occur, and another handler should take over the request.
  bool HandleAsProxy();

  // Tries to acts as a full-featured proxy, handling both HTML and
  // resources.
  void HandleAsProxyForAll();

  // Attempts to handle this as an in-place resource. Returns false if
  // the in-place handling didn't occur, and another handler should take
  // over the request.
  bool HandleAsInPlace();

  // Unconditionally handles a resource that looks like a .pagespeed. resource,
  // whether the result is success or failure.
  void HandleAsPagespeedResource();

  // Waits for an outstanding fetch (obtained by MakeFetch) to complete.  On
  // failure, a failure response will be sent to the client.  The request is
  // handled unconditionally.
  void WaitForFetch();

  // Loads the URL based on the fetchers and other infrastructure in the
  // factory, returning true if the request was handled.  This is used
  // both for slurping and for handling URLs ending with proxy_suffix.
  bool ProxyUrl();

  // Checks to see whether the configuration has set up cookie-based
  // proxy authentication.  If so, and the cookies are not present,
  // clients will be redirected to a page where the cookies can be
  // obtained.  Returns true if the client is authorized for proxying.
  // Return false and responds to the request_ if the client was not
  // authorized.
  bool AuthenticateProxy();

  RequestHeaders* ReleaseRequestHeaders() { return request_headers_.release(); }

  // Returns the options, whether they were custom-computed due to htaccess
  // file, query params, or headers, or were the default options for the vhost.
  const ApacheConfig* options() { return options_; }

  // Was this request made by mod_pagespeed itself? If so, we should not try to
  // handle it, just let Apache deal with it like normal.
  static bool is_pagespeed_subrequest(request_rec* request);

  // Handle mod_pagespeed-specific requests. Handles both .pagespeed. rewritten
  // resources and /mod_pagespeed_statistics, /mod_pagespeed_beacon, etc.
  static apr_status_t instaweb_handler(request_rec* request);

  // Save the original URL as a request "note" before mod_rewrite has
  // a chance to corrupt mod_pagespeed's generated URLs, which would
  // prevent instaweb_handler from being able to decode the resource.
  static apr_status_t save_url_hook(request_rec *request);

  // Implementation of the Apache 'translate_name' hook. Used by the actual hook
  // 'save_url_hook' and directly when we already have the server context.
  static apr_status_t save_url_in_note(request_rec *request,
                                       ApacheServerContext* server_context);

  // By default, apache imposes limitations on URL segments of around
  // 256 characters that appear to correspond to filename limitations.
  // To prevent that, we hook map_to_storage for our own purposes.
  static apr_status_t instaweb_map_to_storage(request_rec* request);

  // This must be called on any InPlaceResourceRecorder allocated by
  // instaweb_handler before calling DoneAndSetHeaders() on it.
  static void AboutToBeDoneWithRecorder(request_rec* request,
                                        InPlaceResourceRecorder* recorder);

 private:
  // Evaluate custom_options based upon global_options, directory-specific
  // options and query-param/request-header options. Stores computed options
  // in custom_options_ if needed.  Sets options_ to point to the correct
  // options to use.
  void ComputeCustomOptions();

  static bool IsCompressibleContentType(const char* content_type);

  static void send_out_headers_and_body(
      request_rec* request,
      const ResponseHeaders& response_headers,
      const GoogleString& output);

  // Determines whether the url can be handled as a mod_pagespeed or in-place
  // optimized resource, and handles it, returning true.  Success status is
  // written to the status code in the response headers.
  static bool handle_as_resource(ApacheServerContext* server_context,
                                 request_rec* request,
                                 GoogleUrl* gurl);

  // Write response headers and send out headers and output, including
  // the option for a custom Content-Type.
  static void write_handler_response(const StringPiece& output,
                                     request_rec* request,
                                     ContentType content_type,
                                     const StringPiece& cache_control);
  static void write_handler_response(const StringPiece& output,
                                     request_rec* request);

  // Returns request URL if it was a .pagespeed. rewritten resource URL.
  // Otherwise returns NULL. Since other Apache modules can change request->uri,
  // we stow the original request URL in a note. This method reads that note
  // and thus should return the URL that the browser actually requested (rather
  // than a mod_rewrite altered URL).
  static const char* get_instaweb_resource_url(
      request_rec* request, ApacheServerContext* server_context);

  // Helper function to support the LogRequestHeadersHandler.  Called
  // once for each header to write header data in a form suitable for
  // javascript inlining.  Used only for tests.
  static int log_request_headers(void* logging_data, const char* key,
                                 const char* value);

  static void instaweb_static_handler(request_rec* request,
                                      ApacheServerContext* server_context);

  static apr_status_t instaweb_statistics_handler(
      request_rec* request, ApacheServerContext* server_context,
      ApacheRewriteDriverFactory* factory);

  // Append the query params from a request into data. This just
  // parses the query params from a request URL. For parsing the query
  // params from a POST body, use parse_body_from_post(). Return true
  // if successful, otherwise, returns false and sets ret to the
  // appropriate status.
  static bool parse_query_params(const request_rec* request, GoogleString* data,
                                 apr_status_t* ret);

  // Read the body from a POST request and append to data. Return true
  // if successful, otherwise, returns false and sets ret to the
  // appropriate status.
  static bool parse_body_from_post(const request_rec* request,
                                   GoogleString* data, apr_status_t* ret);

  static apr_status_t instaweb_beacon_handler(
      request_rec* request, ApacheServerContext* server_context);

  static bool IsBeaconUrl(const RewriteOptions::BeaconUrl& beacons,
                          const GoogleUrl& gurl);

  request_rec* request_;
  RequestContextPtr request_context_;
  ApacheRequestContext* apache_request_context_;  // owned by request_context_.
  ApacheServerContext* server_context_;
  scoped_ptr<RequestHeaders> request_headers_;
  scoped_ptr<ResponseHeaders> response_headers_;
  GoogleString original_url_;
  GoogleUrl stripped_gurl_;  // Any PageSpeed query params are removed.
  scoped_ptr<ApacheConfig> custom_options_;

  // These options_ can be in one of three states:
  //   - they can point to the config's global_options
  //   - they can point to the custom_options_
  //   - after driver creation, they can point to the rewrite_driver_->options()
  // Thus this set of options is not owned by this class.
  //
  // In all three of these states, the pointer and semantics will always be
  // the same.  Only the ownership changes.
  const ApacheConfig* options_;
  RewriteDriver* rewrite_driver_;
  bool driver_owned_;
  int num_response_attributes_;
  RewriteQuery rewrite_query_;
  ApacheFetch* fetch_;

  DISALLOW_COPY_AND_ASSIGN(InstawebHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_INSTAWEB_HANDLER_H_
