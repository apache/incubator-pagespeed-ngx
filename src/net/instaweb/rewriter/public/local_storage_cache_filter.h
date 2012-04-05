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

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_LOCAL_STORAGE_CACHE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_LOCAL_STORAGE_CACHE_FILTER_H_

#include <set>

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class HtmlElement;
class RewriteDriver;

/*
 * The Local Storage Cache rewriter reduces HTTP requests by inlining resources
 * and using browser-side javasript to store the inlined resources in local
 * storage. The javascript also creates a cookie that reflects the resources it
 * has in local storage. On a repeat view, the server uses the cookie to
 * determine if it should replace an inlined resource with a script snipper
 * that loads the resource from local storage. In effect, we get browser
 * caching of inlined resources, theoretically speeding up first view (by
 * inlining) and repeat view (by not resending the inlined resource).
 */
class LocalStorageCacheFilter : public RewriteFilter {
 public:
  static const char kLscCookieName[];
  static const char kLscInitializer[];  // public for the test harness only.

  // State information for an inline filter using LSC.
  class InlineState {
   public:
    InlineState() : initialized_(false), enabled_(false) {}

   private:
    friend class LocalStorageCacheFilter;

    bool initialized_;
    bool enabled_;
    GoogleString url_;
  };

  explicit LocalStorageCacheFilter(RewriteDriver* rewrite_driver);
  virtual ~LocalStorageCacheFilter();

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "LocalStorageCache"; }
  virtual const char* id() const {
    return RewriteOptions::kLocalStorageCacheId;
  }

  std::set<StringPiece>* mutable_cookie_hashes() { return &cookie_hashes_; }

  // Tell the LSC that the resource with the given url is a candidate for
  // storing in the browser's local storage. If LSC is disabled it's a no-op,
  // otherwise it determines the LSC's version of the url and, if
  // skip_cookie_check is true or the hash of the LSC's url is in the LSC's
  // cookie, adds the LSC's url as an attribute of the given element, which the
  // LSC later uses to tell that the element is suitable for storing in local
  // cache. Returns true if the attribute was added. Saves various computed
  // values in the given state variable for any subsequent call (a filter might
  // need to call this method once with skip_cookie_check false, then again
  // later with it true).
  // url is the URL from the HTML element, src from img, href from style.
  // driver is the request's context.
  // is_enabled is set to true if the local storage cache filter is enabled.
  // skip_cookie_check if true skips the checking of the cookie for the hash
  //                   and adds the LSC's url attribute if LSC is enabled.
  // element is the element to add the attribute to.
  // state is where to save the computed values.
  static bool AddStorableResource(const StringPiece& url,
                                  RewriteDriver* driver,
                                  bool skip_cookie_check,
                                  HtmlElement* element,
                                  InlineState* state);

  // Tell the LSC to add its attributes to the given element: pagespeed_lsc_url
  // (if not already added [has_url is false]), pagespeed_lsc_hash, and, if the
  // resource has an expiry time [in cached], pagespeed_lsc_expiry. This is a
  // no-op if LSC is disabled.
  // url is the URL of the resource being rewritten.
  // cached is the result of the resource rewrite.
  // has_url is true if the element already has an url so don't add it again.
  // driver is the request's context.
  // element is the element to update.
  // Returns true if the element was updated.
  static bool AddLscAttributes(const StringPiece url,
                               const CachedResult& cached,
                               bool has_url,
                               RewriteDriver* driver,
                               HtmlElement* element);

  // Remove the LSC attributes from the given element.
  static void RemoveLscAttributes(HtmlElement* element);

 private:
  void InsertOurScriptElement(HtmlElement* before);
  static bool IsHashInCookie(const RewriteDriver* driver,
                             const StringPiece cookie_name,
                             const StringPiece hash,
                             std::set<StringPiece>* hash_set);
  static GoogleString ExtractOtherImgAttributes(const HtmlElement* element);

  // Have we inserted the script of utility functions?
  bool script_inserted_;
  // Have we seen any inlined resources that need the utility functions?
  bool script_needs_inserting_;
  // The set of hashes in the local storage cache cookie. Each element points
  // into the rewrite driver's cookies() - that must not change underneath us.
  std::set<StringPiece> cookie_hashes_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCacheFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_LOCAL_STORAGE_CACHE_FILTER_H_
