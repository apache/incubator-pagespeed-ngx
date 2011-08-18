// Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;
class QueryParams;

class RewriteQuery {
 public:
  // The names of query-params.
  static const char kModPagespeed[];
  static const char kModPagespeedCssInlineMaxBytes[];
  static const char kModPagespeedDisableForBots[];
  static const char kModPagespeedFilters[];

  // Scans query parameters for "ModPagespeed" flags, returning a populated
  // RewriteOptions object if there were any "ModPagespeed" flags found, and
  // they were all parsed successfully.  If any were parsed unsuccessfully,
  // or there were none found, then NULL is returned.
  //
  // The caller must delete the returned options when done.
  //
  // TODO(jmarantz): consider allowing an alternative prefix to "ModPagespeed"
  // to accomodate other Page Speed Automatic applications that might want to
  // brand differently.
  static RewriteOptions* Scan(const QueryParams& query_params,
                              MessageHandler* handler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_QUERY_H_
