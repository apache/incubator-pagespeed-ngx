/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_

#include <string>


#include "googleurl/src/gurl.h"

namespace net_instaweb {

namespace GoogleUrl {

// Helper functions around GURL to make it a little easier to use
inline std::string Spec(const GURL& gurl) { return gurl.spec(); }

// For "http://a.com/b/c/d?e=f/g returns "http://a/b/c", omitting trailing slash.
std::string AllExceptLeaf(const GURL& gurl);

// For "http://a.com/b/c/d?e=f/g returns "d?e=f/g", omitting leading slash.
std::string Leaf(const GURL& gurl);

// For "http://a.com/b/c/d?e=f/g returns "http://a.com" without trailing slash
std::string Origin(const GURL& gurl);

// For "http://a.com/b/c/d?E=f/g returns "/b/c/d?e=f/g" including leading slash
std::string PathAndLeaf(const GURL& gurl);

}  // namespace GoogleUrl

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_GOOGLE_URL_H_
