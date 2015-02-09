/*
 * Copyright 2014 Google Inc.
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

// Author: stevensr@google.com (Ryan Stevens)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

// A mobile role and its associated HTML attribute value.
struct MobileRoleData {
  static const MobileRoleData kMobileRoles[MobileRole::kInvalid];

  MobileRoleData(MobileRole::Level level, const char* value)
      : level(level),
        value(value) { }

  static const MobileRoleData* FromString(const StringPiece& mobile_role);
  static MobileRole::Level LevelFromString(const StringPiece& mobile_role);
  static const char* StringFromLevel(MobileRole::Level level) {
    return (level < MobileRole::kInvalid) ? kMobileRoles[level].value : NULL;
  }

  const MobileRole::Level level;
  const char* const value;  // Set to a static string in cc.
};

// Rewrite HTML to be mobile-friendly based on "data-mobile-role" attributes in
// the HTML tags. To reorganize the DOM, the filter puts containers at the end
// of the body into which we move tagged elements. The containers are later
// removed after the filter is done processing the document body. The filter
// applies the following transformations:
//  - Add mobile <style> and <meta name="viewport"...> tags to the head.
//  - Remove all table tags (but keep the content). Almost all tables in desktop
//    HTML are for formatting, not displaying data, and they tend not to resize
//    well for mobile. The easiest thing to do is to simply strip out the
//    formatting and hope the content reflows properly.
//  - Reorder body of the HTML DOM elements based on mobile role. Any elements
//    which don't have an important parent will get removed, except for a
//    special set of "keeper" tags (like <script> or <style>). The keeper tags
//    are retained because they are often necessary for the website to work
//    properly, and because they have no visible appearance on the page.
//  - Remove all elements from inside data-mobile-role="navigational" elements
//    except in a special set of nav tags (notably <a>). Nav sections often do
//    not resize well due to fixed width formatting and drop-down menus, so it
//    is often necessary to pull out what you want, instead of shuffling around
//    what is there.
//
// Remaining todos:
//  - TODO (stevensr): This script does not handle flush windows in the body.
//  - TODO (stevensr): It would be nice to tweak the table-xform behavior via
//    options. Also, there has been mention that removing tables across flush
//    windows could be problematic. This should be addressed at some point.
//  - TODO (stevensr): Enable this filter only for mobile UAs, and have a query
//    param option to turn it on for all UAs for debugging.
//  - TODO (stevensr): Write pcache entry if rewriting page fails. We should
//    then probably inject some JS to auto-refresh the page so the user does not
//    see the badly rewritten result.
//  - TODO (stevensr): Add a separate wildcard option to allow/disallow URLs
//    from using this filter. Of course sites can use our existing Allow and
//    Disallow directives but that turns off all optimizations, and this one is
//    one that might be extra finicky (e.g. don't touch my admin pages).
//  - TODO (stevensr): Turn on css_move_to_head_filter.cc to reorder elements
//    we inject into the head.
class MobilizeRewriteFilter : public CommonFilter {
 public:
  static const char kPagesMobilized[];
  static const char kKeeperBlocks[];
  static const char kHeaderBlocks[];
  static const char kNavigationalBlocks[];
  static const char kContentBlocks[];
  static const char kMarginalBlocks[];
  static const char kDeletedElements[];
  static const char kSetSpacerHeight[];

  // Static list of tags we keep without traversing.  Public so
  // MobilizeLabelFilter knows which tags to ignore.
  static const HtmlName::Keyword kKeeperTags[];
  static const int kNumKeeperTags;

  explicit MobilizeRewriteFilter(RewriteDriver* rewrite_driver);
  virtual ~MobilizeRewriteFilter();

  static void InitStats(Statistics* statistics);

  virtual void DetermineEnabled(GoogleString* disabled_reason);
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual const char* Name() const { return "MobilizeRewrite"; }

 private:
  void AppendStylesheet(const StringPiece& css_file_name,
                        StaticAssetEnum::StaticAsset asset,
                        HtmlElement* element);
  void AddStyle(HtmlElement* element);
  MobileRole::Level GetMobileRole(HtmlElement* element);
  void AddStaticScript(StringPiece script);

  bool CheckForKeyword(
      const HtmlName::Keyword* sorted_list, int len, HtmlName::Keyword keyword);
  void LogEncounteredBlock(MobileRole::Level level);

  int body_element_depth_;
  int keeper_element_depth_;
  bool reached_reorder_containers_;
  bool added_viewport_;
  bool added_style_;
  bool added_containers_;
  bool added_mob_js_;
  bool added_progress_;
  bool added_spacer_;
  bool in_script_;
  bool use_js_layout_;
  bool use_js_nav_;
  bool use_static_;
  bool rewrite_js_;
  GoogleString static_file_prefix_;

  // Statistics
  // Number of web pages we have mobilized.
  Variable* num_pages_mobilized_;
  // Number of blocks of each mobile role encountered and reordered.
  Variable* num_keeper_blocks_;
  Variable* num_header_blocks_;
  Variable* num_navigational_blocks_;
  Variable* num_content_blocks_;
  Variable* num_marginal_blocks_;
  // Number of elements deleted.
  Variable* num_elements_deleted_;

  // Used for overriding default behavior in testing.
  friend class MobilizeRewriteFilterTest;

  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_
