/*
 * Copyright 2015 Google Inc.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_FILTER_H_

#include <map>
#include <vector>

#include "net/instaweb/rewriter/mobilize_menu.pb.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/mobilize_filter_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

// Extract navigational menus from a page labeled with
// data_mobile_role='navigational' annotations by MobilizeLabelFilter into a
// menu protobuf suitable for injection by MobilizeRewriteFilter.  The flow goes
// as follows:
// 1 - MobilizeLabelFilter traverses the entire document.  When it reaches
//     EndDocument, it computes mobile roles and adds them to DOM elements.
//     This means at the moment that those DOM elements must all be in the flush
//     window (in IFrame mode RenderBlockingHtmlComputation ensures this).
// 2 - *After* MobilizeLabelFilter calls EndDocument, MobilizeMenuFilter can
//     *start* traversing the DOM, finding and extracting a menu proto.
// 3 - On reaching EndDocument, MobilizeMenuFilter cleans up the menu.  It can
//     then be handed off to MobilizeMenuRenderFilter.  It inserts the menu at
//     document end (or during Render for iframed pages).
// As you can see, these three filters are separate because:
// 1 - We must completely traverse a page before we can label it (so label
//     filter must run to completion before anything else can start work).
// 2 - Label and Menu extraction might happen on a different page from
//     Menu Render if we are running in iframe mode.
// TODO(jmaessen): How do we make this flush tolerant outside iframe mode?  It
// seems like we'll need to fall back to JavaScript in the face of flushes
// today, at least for the first couple of page views.  The problem is we'll see
// all but one window of content before the label filter has computed labels.
// PCache storage of labeling will mitigate this, but we won't get a menu until
// the second page visit.  If we could selectively disable flushing we could
// also make this work.  Finally, we could just fetch the page a second time
// as a resource and trust (as we already do) that its menus will match.
class MobilizeMenuFilter : public MobilizeFilterBase {
 public:
  explicit MobilizeMenuFilter(RewriteDriver* rewrite_driver);
  virtual ~MobilizeMenuFilter();

  static void InitStats(Statistics* statistics);
  // Run menu cleanup by hand.  Exposed for testing, implicit unless you
  // set_cleanup_menu(false)
  static void CleanupMenu(MobilizeMenu* menu);
  // Check well-formedness of a cleaned-up menu for debug purposes.
  static bool IsMenuOk(const MobilizeMenu& menu);

  virtual void DetermineEnabled(GoogleString* disabled_reason);
  virtual void StartDocumentImpl();
  virtual void EndDocumentImpl();
  virtual void StartNonSkipElement(
      MobileRole::Level role_attribute, HtmlElement* element);
  virtual void EndNonSkipElement(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual const char* Name() const { return "MobilizeMenu"; }

  // Get the constructed menu.
  const MobilizeMenu& menu() { return *menu_; }
  // Release the constructed menu.
  const MobilizeMenu* release_menu() { return menu_.release(); }

  // Set whether to cleanup menus (for testing purposes, defaults to true)
  void set_cleanup_menu(bool s) { cleanup_menu_ = s; }

 private:
  typedef std::map<GoogleString, int> UrlLevelMap;

  static void SweepNestedMenu(const MobilizeMenu& menu, MobilizeMenu* new_menu);
  static void SweepMenu(const MobilizeMenu& menu, MobilizeMenu* new_menu);
  static void CollectMenuUrls(
      int level, const MobilizeMenu& menu, UrlLevelMap* url_level);
  static void ClearDuplicateEntries(
      int level, MobilizeMenu* menu, UrlLevelMap* url_level);

  void StartTopMenu();
  void StartDeepMenu();
  void ClearMenuText();
  void EndMenuCommon();
  void EndTopMenu();
  void EndDeepMenu();
  MobilizeMenuItem* EnsureMenuItem();
  void StartMenuItem(const char* href_or_null);
  void EndMenuItem();

  HtmlElement* outer_nav_element_;
  scoped_ptr<MobilizeMenu> menu_;
  GoogleString menu_item_text_;
  bool menu_item_trailing_whitespace_;
  // The following points to elements of menu_.
  std::vector<MobilizeMenu*> menu_stack_;

  bool cleanup_menu_;

  DISALLOW_COPY_AND_ASSIGN(MobilizeMenuFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_FILTER_H_
