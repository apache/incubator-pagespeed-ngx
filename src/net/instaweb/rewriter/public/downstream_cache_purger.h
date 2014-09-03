/*
 * Copyright 2013 Google Inc.
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

// Author: anupama@google.com (Anupama Dutta)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHE_PURGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHE_PURGER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {
class GoogleUrl;
class RewriteDriver;

// This class acts as a wrapper class for issuing purge requests to downstream
// caches.
class DownstreamCachePurger {
 public:
  explicit DownstreamCachePurger(RewriteDriver* driver);
  ~DownstreamCachePurger();

  // Issues the purge request if the request headers, the purge-related options
  // and the amount of rewriting completed satisfy certain conditions.
  // Returns true if purge request was issued and false otherwise.
  bool MaybeIssuePurge(const GoogleUrl& google_url);

  void Clear();

 private:
  // Check rewrite options specified for downstream caching behavior and
  // amount of rewriting initiated and completed to decide whether the
  // fully rewritten response is significantly better than the stored
  // version and whether the currently stored version ought to be purged.
  bool ShouldPurgeRewrittenResponse(const GoogleUrl& google_url);

  // Construct the purge URL and decide on the purge HTTP method (GET, PURGE
  // etc.) based on the rewrite options.
  bool GeneratePurgeRequestParameters(const GoogleUrl& page_url);

  // Initiates a purge request fetch.
  void PurgeDownstreamCache();

  RewriteDriver* driver_;
  GoogleString purge_url_;
  GoogleString purge_method_;

  // Set to true if we already made a purge request to the downstream cache,
  // so we don't keep trying to do it. Note that there is never more than one
  // DownstreamCachePurger object per RewriteDriver.
  bool made_downstream_purge_attempt_;

  DISALLOW_COPY_AND_ASSIGN(DownstreamCachePurger);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHE_PURGER_H_
