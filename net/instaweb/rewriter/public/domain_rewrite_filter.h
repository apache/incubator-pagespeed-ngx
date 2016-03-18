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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_

#include <utility>
#include <vector>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class Statistics;
class Variable;

// Filter that rewrites URL domains for resources that are not
// otherwise rewritten.  For example, the user may want to
// domain-shard adding a hash to their URL leaves, or domain shard
// resources that are not cacheable.
//
// This will also rewrite hyperlinks and URL-related headers and metas
// if domain_rewrite_hyperlinks() is on, and also to Set-Cookie headers if
// domain_rewrite_cookies() is on.
class DomainRewriteFilter : public CommonFilter {
 public:
  static const char kStickyRedirectHeader[];

  DomainRewriteFilter(RewriteDriver* rewrite_driver, Statistics* stats);
  ~DomainRewriteFilter();
  static void InitStats(Statistics* statistics);
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const { return "DomainRewrite"; }

  // Injects scripts only when option ClientDomainRewrite is true, and
  // the current document is not AMP.
  ScriptUsage GetScriptUsage() const override { return kMayInjectScripts; }

  enum RewriteResult {
    kRewroteDomain,
    kDomainUnchanged,
    kFail,
  };

  enum HeaderSource {
    kHttp,
    kMetaHttpEquiv
  };

  typedef std::vector<std::pair<StringPiece, StringPiece> > SetCookieAttributes;

  // Rewrites the specified URL (which might be relative to the base tag)
  // into an absolute sharded url.
  //
  // Absolute URL output_url will be set if kRewroteDomain or
  // kDomainUnchanged returned.
  //
  // static for use in UpdateLocationHeader.
  static RewriteResult Rewrite(const StringPiece& input_url,
                               const GoogleUrl& base_url,
                               const ServerContext* server_context,
                               const RewriteOptions* options,
                               bool apply_sharding,
                               bool apply_domain_suffix,
                               GoogleString* output_url);

  // Update url and domains in headers as per the rewrite rules configured
  // for this domain.
  //
  // For now this fixes Location:, Refresh:, and Set-Cookie:
  //
  // static since we may need to do it in Apache with no appropriate
  // RewriteDriver available.
  static void UpdateDomainHeaders(const GoogleUrl& base_url,
                                  const ServerContext* server_context,
                                  const RewriteOptions* options,
                                  ResponseHeaders* headers);

  // Update an indivual header based on domain rewrite rules. Returns true
  // if a change was made, in which case *out should be committed.
  static bool UpdateOneDomainHeader(HeaderSource src,
                                    const GoogleUrl& base_url,
                                    const ServerContext* server_context,
                                    const RewriteOptions* options,
                                    StringPiece name,
                                    StringPiece value_in,
                                    GoogleString* out);

  // Like UpdateOneDomainHeader, but specifically for Set-Cookie.
  static bool UpdateSetCookieHeader(const GoogleUrl& base_url,
                                    const ServerContext* server_context,
                                    const RewriteOptions* options,
                                    StringPiece value_in,
                                    GoogleString* out);


  // Tries to parse the content of a Refresh header, returning if successful.
  // *before gets anything before the URL or its opening quotes. *url gets the
  // portion of parse that's the URL itself, and *after gets everything after
  // the URL and its closing quote, if any. Note that this means that
  // reassembling the URL may require addition of new quotes and escaping.
  static bool ParseRefreshContent(StringPiece input,
                                  StringPiece* before,
                                  StringPiece* url,
                                  StringPiece* after);


  // Tries to parse the contents of a Set-Cookie header, specified as "input",
  // extracting out the cookie string into *cookie_string and the attribute
  // key value pairs into *attributes. This does not eliminate duplicate
  // attributes; note that the spec requires using the last occurrences.
  // *attributes and *cookie_string are overwritten, not appended to.
  static void ParseSetCookieAttributes(StringPiece input,
                                       StringPiece* cookie_string,
                                       SetCookieAttributes* attributes);

 private:
  static void TryUpdateOneHttpDomainHeader(const GoogleUrl& base_url,
                                           const ServerContext* server_context,
                                           const RewriteOptions* options,
                                           StringPiece name,
                                           ResponseHeaders* headers);

  // Stats on how much domain-rewriting we've done.
  Variable* rewrite_count_;

  DISALLOW_COPY_AND_ASSIGN(DomainRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_
