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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class OutputResource;
class RewriteContext;
class RewriteDriver;
class UrlSegmentEncoder;

class RewriteFilter : public CommonFilter {
 public:
  explicit RewriteFilter(RewriteDriver* driver)
      : CommonFilter(driver) {
  }
  virtual ~RewriteFilter();

  virtual const char* id() const = 0;

  // Create an input resource by decoding output_resource using the
  // filter's. Assures legality by explicitly permission-checking the result.
  ResourcePtr CreateInputResourceFromOutputResource(
      OutputResource* output_resource);

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

  // Add this filter to the logged list of applied rewriters.
  // This class logs using id().
  virtual void LogFilterModifiedContent();

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
