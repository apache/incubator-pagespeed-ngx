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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SINGLE_REWRITE_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SINGLE_REWRITE_CONTEXT_H_

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class OutputPartition;
class OutputPartitions;
class ResourceContext;
class RewriteDriver;

// Class that unifies tasks common to building rewriters for filters
// that only do one-for-one URL substitutions.
//
// TODO(jmarantz): Sever the ties to RewriteSingleResourceFilter.
class SingleRewriteContext : public RewriteContext {
 public:
  // Transfers ownership of resource_context, which must be NULL or
  // allocated with 'new'.
  SingleRewriteContext(RewriteDriver* driver, RewriteContext* parent,
                       ResourceContext* resource_context);
  virtual ~SingleRewriteContext();

 protected:
  // Subclasses of SingleRewriteContext must override this:
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output) = 0;

  // SingleRewriteContext takes care of these methods from RewriteContext:
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs);
  virtual void Rewrite(OutputPartition* partition,
                       const OutputResourcePtr& output);

  DISALLOW_COPY_AND_ASSIGN(SingleRewriteContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SINGLE_REWRITE_CONTEXT_H_
