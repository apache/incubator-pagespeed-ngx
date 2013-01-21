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
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_APACHE_APACHE_SLURP_H_
#define NET_INSTAWEB_APACHE_APACHE_SLURP_H_

struct request_rec;

namespace net_instaweb {

class ApacheServerContext;

// Loads the URL based on the fetchers and other infrastructure in the
// factory.
void SlurpUrl(ApacheServerContext* manager, request_rec* r);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_SLURP_H_
