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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/url_segment_encoder.h"

namespace net_instaweb {

class RewriteFilter : public CommonFilter {
 public:
  explicit RewriteFilter(RewriteDriver* driver)
      : CommonFilter(driver) {
  }
  virtual ~RewriteFilter();

  virtual const char* id() const = 0;

  // Override DetermineEnabled so that filters that use the DOM cohort of the
  // property cache can enable writing of it in the RewriterDriver. Filters
  // inheriting from RewriteDriver that use the DOM cohort should override
  // UsePropertyCacheDomCohort to return true.
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  // Returns whether this filter can modify urls.  Because most filters do
  // modify urls this defaults returning true, and filters that commit to never
  // modifying urls should override it to return false.
  virtual bool CanModifyUrls() { return true; }

  // All RewriteFilters define how they encode URLs and other
  // associated information needed for a rewrite into a URL.
  // The default implementation handles a single URL with
  // no extra data.  The filter owns the encoder.
  virtual const UrlSegmentEncoder* encoder() const;

  // If this method returns true, the data output of this filter will not be
  // cached, and will instead be recomputed on the fly every time it is needed.
  // (However, the transformed URL and similar metadata in CachedResult will be
  //  kept in cache).
  //
  // The default implementation returns false.
  virtual bool ComputeOnTheFly() const;

  // Generates a RewriteContext appropriate for this filter.
  // Default implementation returns NULL.  This must be overridden by
  // filters.  This is used to implement Fetch.
  virtual RewriteContext* MakeRewriteContext();

  // Generates a nested RewriteContext appropriate for this filter.
  // Default implementation returns NULL.
  // This is used to implement ajax rewriting.
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

  // Encodes user agent information needed by the filter into ResourceContext.
  // See additional header document for
  // RewriteContext::EncodeUserAgentIntoResourceContext.
  virtual void EncodeUserAgentIntoResourceContext(
      ResourceContext* context) const {}

  // Determine the charset of a script. Logic taken from:
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //   scripting-1.html#establish-script-block-source
  // 1. If the script has a Content-Type with a charset, use that, else
  // 2. If the script has a charset attribute, use that, else
  // 3. If the script has a BOM, use that, else
  // 4. Use the charset of the enclosing page.
  // If none of these are specified we return StringPiece(NULL).
  // Note that Chrome and Opera do not actually implement this spec - it seems
  // that for them a BOM overrules a charset attribute (swap rules 2 and 3).
  // Note that the return value might point into one of the given arguments so
  // you must ensure that it isn't used past the life of any of the arguments.
  static StringPiece GetCharsetForScript(const Resource* script,
                                         const StringPiece attribute_charset,
                                         const StringPiece enclosing_charset);

  // Determine the charset of a stylesheet. Logic taken from:
  //   http://www.opentag.com/xfaq_enc.htm#enc_howspecifyforcss
  // with the BOM rule below added somewhat arbitrarily. In essence, we take
  // the -last- charset we see, if you pretend that headers come last.
  // 1. If the stylesheet has a Content-Type with a charset, use that, else
  // 2. If the stylesheet has an initial @charset, use that, else
  // 3. If the stylesheet has a BOM, use that, else
  // 4. If the style element has a charset attribute, use that, else
  // 5. Use the charset of the enclosing page.
  // If none of these are specified we return StringPiece(NULL).
  // Note that I do not know which browsers implement this, but I know they
  // aren't consistent, so some definitely don't.
  static GoogleString GetCharsetForStylesheet(
      const Resource* stylesheet,
      const StringPiece attribute_charset,
      const StringPiece enclosing_charset);

  // Determines which filters are related to this RewriteFilter.  Note,
  // for example, that the ImageRewriteFilter class implements lots of
  // different RewriteOptions::Filters.
  //
  // This is used for embedding the relevant enabled filter IDs.  See
  // the doc for RewriteOptions::add_options_to_urls_.  We want to support
  // that without bloating URLs excessively adding unrelated filter settings.
  //
  // The vector is returned in numerically increasing order so binary_search
  // is possible.
  //
  // *num_filters is set to the size of this array.
  //
  // Ownership of the filter-vector is not transferred to the caller; it
  // is expected to return a pointer to a static vector.
  virtual const RewriteOptions::Filter* RelatedFilters(int* num_filters) const;

  // Return the names of options related to this RewriteFilter in
  // case-insensitive alphabetical order. NULL means there are none.
  // Ownership of the vector is not transferred to the caller.
  virtual const StringPieceVector* RelatedOptions() const {
    return NULL;
  }

 protected:
  // This class logs using id().
  virtual const char* LoggingId() { return id(); }

 private:
  // Filters should override this and return true if they write to the property
  // cache DOM cohort. This is so that the cohort is only written if a filter is
  // enabled that actually makes use of it to prevent filling the cache with a
  // large amount of useless entries.
  virtual bool UsesPropertyCacheDomCohort() const { return false; }

  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
