// Copyright 2011 Google Inc. All Rights Reserved.
// Author: gagansingh@google.com (Gagan Singh)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_

#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class Layout;
class GoogleUrl;
class PublisherConfig;

namespace BlinkUtil {

// Finds the layout for the given request_url.
const Layout* FindLayout(const PublisherConfig& config,
                         const GoogleUrl& request_url);

}  // namespace BlinkUtil

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
