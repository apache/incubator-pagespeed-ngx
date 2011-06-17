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

#include "net/instaweb/rewriter/public/single_rewrite_context.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"  // for GoogleString, NULL

namespace net_instaweb {

class RewriteContext;
class RewriteDriver;

SingleRewriteContext::SingleRewriteContext(RewriteDriver* driver,
                                           RewriteContext* parent,
                                           ResourceContext* resource_context)
    : RewriteContext(driver, parent, resource_context) {
}

SingleRewriteContext::~SingleRewriteContext() {
}

bool SingleRewriteContext::Partition(OutputPartitions* partitions,
                                     OutputResourceVector* outputs) {
  bool ret = false;
  if (num_slots() == 1) {
    ret = true;
    ResourcePtr resource(slot(0)->resource());
    if (resource->IsValidAndCacheable()) {
      OutputResourcePtr output_resource(
          Manager()->CreateOutputResourceFromResource(
              Options(), id(), encoder(), resource_context(),
              resource, kind(), true /* async flow */));
      if (output_resource.get() != NULL) {
        OutputPartition* partition = partitions->add_partition();
        resource->AddInputInfoToPartition(0, partition);
        output_resource->set_cached_result(partition->mutable_result());
        outputs->push_back(output_resource);
      }
    }
  }
  return ret;
}

void SingleRewriteContext::Rewrite(int partition_index,
                                   OutputPartition* partition,
                                   const OutputResourcePtr& output_resource) {
  CHECK_EQ(0, partition_index);
  ResourcePtr resource(slot(0)->resource());
  CHECK(resource.get() != NULL);
  CHECK(resource->loaded());
  CHECK(resource->ContentsValid());
  CHECK(output_resource.get() != NULL);
  output_resource->set_cached_result(partition->mutable_result());
  RewriteSingle(resource, output_resource);
}

}  // namespace net_instaweb
