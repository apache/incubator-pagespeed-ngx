/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_GFLAGS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_GFLAGS_H_

#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class RewriteDriverFactory;

// Implements rewriter options as command-line options, using the
// Google "gflags" package.
class RewriteGflags {
 public:
  // The constructor parses the options.
  RewriteGflags(const char* progname, int* argc, char*** argv);

  // Apply the parsed options to a factory.
  bool SetOptions(RewriteDriverFactory* factory) const;

  // LRU size is potentially needed at factory construction time so it
  // is exposed as a method.
  int64 lru_cache_size_bytes() const;

  // Determines whether a flag was explicitly set, as opposed to having its
  // default value.
  bool WasExplicitlySet(const char* name) const;

 private:
  // There is no data in this class because the underlying gflags
  // class holds the parsed options in globals.

  DISALLOW_COPY_AND_ASSIGN(RewriteGflags);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_GFLAGS_H_
