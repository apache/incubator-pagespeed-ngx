// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_RULES_ENABLE_GZIP_COMPRESSION_H_
#define PAGESPEED_RULES_ENABLE_GZIP_COMPRESSION_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "pagespeed/rules/minify_rule.h"
#include "pagespeed/rules/savings_computer.h"

typedef struct z_stream_s z_stream;

namespace pagespeed {

class PagespeedInput;
class Resource;
class Results;

namespace rules {

/**
 * Lint rule that checks that text resources are compressed before
 * they are sent over the wire.
 */
class EnableGzipCompression : public MinifyRule {
 public:
  // @param computer Object responsible for computing the compressed
  // size of the resource.
  EnableGzipCompression();
  virtual int ComputeScore(const InputInformation& input_info,
                           const RuleResults& results);

 private:
  DISALLOW_COPY_AND_ASSIGN(EnableGzipCompression);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_ENABLE_GZIP_COMPRESSION_H_
