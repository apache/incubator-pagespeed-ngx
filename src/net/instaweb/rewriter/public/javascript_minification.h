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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_MINIFICATION_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_MINIFICATION_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
// Lexically minify Javascript input, appending the results to output.
// We are subject to some constraints not shared by tools such as jsMin that
// are used by a content producer:
//   * We must preserve the semantics of all input code.  jsMin has documented
//     problems with eg a + ++b, which it minifies to a+++b.
//   * We want to preserve (at least) copyright notices in comments.
//   ** TODO(jmaessen): Determine if we need to preserve js code in comments.
// As a result we do a more thorough/careful scan, though we still avoid
// parsing.  We may in future parse brace hierarchies in order to provide
// definition separation.
//
// TODO(jmaessen): Actual minifier.  Just a stub.
void MinifyJavascript(const StringPiece& input, std::string* output);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_MINIFICATION_H_
