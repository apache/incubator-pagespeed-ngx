// Copyright 2011 Google Inc. All Rights Reserved.
// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/blink_util.h"

#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {
namespace BlinkUtil {

const Layout* FindLayout(const PublisherConfig& config,
                         const GoogleUrl& request_url) {
  for (int i = 0; i < config.layout_size(); i++) {  // Typically 3-4 layouts.
    const Layout& layout = config.layout(i);
    if (layout.reference_page_url_path() == request_url.PathAndLeaf()) {
      return &layout;
    }
    for (int j = 0; j < layout.relative_url_patterns_size(); j++) {
      Wildcard wildcard(layout.relative_url_patterns(j));
      if (wildcard.Match(request_url.PathAndLeaf())) {
        return &layout;
      }
    }
  }

  return NULL;
}

}  // namespace PanelUtil
}  // namespace net_instaweb
