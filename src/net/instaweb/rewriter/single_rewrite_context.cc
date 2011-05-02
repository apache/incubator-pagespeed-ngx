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

namespace net_instaweb {

SingleRewriteContext::SingleRewriteContext(RewriteDriver* driver,
                                           const ResourceSlotPtr& slot,
                                           ResourceContext* resource_context)
    : RewriteContext(driver, resource_context) {
  AddSlot(slot);
}

SingleRewriteContext::~SingleRewriteContext() {
}

void SingleRewriteContext::Render(const OutputPartition& partition,
                                  const ResourcePtr& output_resource) {
  // We CHECK num_slots because there's no way we should be creating
  // a SingleRewriteContext with more than one slot.
  CHECK_EQ(1, num_slots());

  // However, we soft-fail on corrupt data read from the cache.
  if ((partition.input_size() == 1) && (partition.input(0) == 0)) {
    ResourceSlotPtr resource_slot(slot(0));
    resource_slot->SetResource(output_resource);
    RenderSlotOnDetach(resource_slot);
  } else {
    // TODO(jmarantz): bump a failure-due-to-corrupt-cache statistic
  }
}

bool SingleRewriteContext::PartitionAndRewrite(OutputPartitions* partitions) {
  // We CHECK num_slots because there's no way we should be creating
  // a RewriteContext for this filter with more than one slot.
  CHECK_EQ(1, num_slots());
  RewriteSingleResourceFilter::RewriteResult result =
      RewriteSingleResourceFilter::kRewriteFailed;
  OutputPartition* partition = partitions->add_partition();

  ResourcePtr resource(slot(0)->resource());
  if ((resource.get() != NULL) && resource->loaded() &&
      resource->ContentsValid()) {
    ResourceManager::Kind kind = ResourceManager::kRewrittenResource;
    if (ComputeOnTheFly()) {
      kind = ResourceManager::kOnTheFlyResource;
    }
    OutputResourcePtr output_resource(
        resource_manager()->CreateOutputResourceFromResource(
            options(), id(), encoder(), resource_context(), resource, kind));
    output_resource->set_cached_result(partition->mutable_result());
    result = Rewrite(resource.get(), output_resource.get());
  }

  bool ret = true;
  switch (result) {
    case RewriteSingleResourceFilter::kRewriteOk:
      partition->add_input(0);
      break;
    case RewriteSingleResourceFilter::kTooBusy:
      ret = false;
      break;
    case RewriteSingleResourceFilter::kRewriteFailed:
      partition->mutable_result()->set_optimizable(false);
      break;
  }
  return ret;
}

}  // namespace net_instaweb
