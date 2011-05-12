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

#include <cstddef>
#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

class RewriteDriver;

SingleRewriteContext::SingleRewriteContext(RewriteDriver* driver,
                                           ResourceContext* resource_context)
    : RewriteContext(driver, resource_context) {
}

SingleRewriteContext::~SingleRewriteContext() {
}

void SingleRewriteContext::Render(const OutputPartition& partition,
                                  const OutputResourcePtr& output_resource) {
  // We CHECK num_slots because there's no way we should be creating
  // a SingleRewriteContext with more than one slot.
  CHECK_EQ(1, num_slots());

  // However, we soft-fail on corrupt data read from the cache.
  if ((partition.input_size() == 1) && (partition.input(0) == 0)) {
    ResourceSlotPtr resource_slot(slot(0));
    ResourcePtr resource(output_resource);
    resource_slot->SetResource(resource);
    RenderSlotOnDetach(resource_slot);
  } else {
    // TODO(jmarantz): bump a failure-due-to-corrupt-cache statistic
  }
}

bool SingleRewriteContext::PartitionAndRewrite(OutputPartitions* partitions,
                                               OutputResourceVector* outputs) {
  bool ret = false;
  if (num_slots() == 1) {
    ResourcePtr resource(slot(0)->resource());
    GoogleUrl gurl(resource->url());
    UrlPartnership partnership(options(), gurl);
    ResourceNamer full_name;
    if (partnership.AddUrl(resource->url(),
                           resource_manager()->message_handler())) {
      const GoogleUrl* mapped_gurl = partnership.FullPath(0);
      GoogleString name;
      StringVector v;
      GoogleString encoded_url;
      v.push_back(mapped_gurl->LeafWithQuery().as_string());
      encoder()->Encode(v, resource_context(), &encoded_url);
      full_name.set_name(encoded_url);
      full_name.set_id(id());
      const ContentType* content_type = resource->type();
      if (content_type != NULL) {
        // TODO(jmaessen): The addition of 1 below avoids the leading ".";
        // make this convention consistent and fix all code.
        full_name.set_ext(content_type->file_extension() + 1);
      }

      OutputResourcePtr output_resource(new OutputResource(
          resource_manager(), gurl.AllExceptLeaf(), full_name, content_type,
          options(), kind()));
      output_resource->set_written_using_rewrite_context_flow(true);
      OutputPartition partition;
      RewriteSingleResourceFilter::RewriteResult result =
        Rewrite(&partition, output_resource);
      if (result == RewriteSingleResourceFilter::kRewriteOk) {
        partition.add_input(0);
        *partitions->add_partition() = partition;
        outputs->push_back(output_resource);
        ret = true;
      } else if (result == RewriteSingleResourceFilter::kRewriteFailed) {
        ret = true;  // write empty partition table
      }
    }
  }
  return ret;
}

RewriteSingleResourceFilter::RewriteResult
SingleRewriteContext::Rewrite(OutputPartition* partition,
                              const OutputResourcePtr& output_resource) {
  RewriteSingleResourceFilter::RewriteResult result =
      RewriteSingleResourceFilter::kRewriteFailed;
  ResourcePtr resource(slot(0)->resource());
  if ((resource.get() != NULL) && resource->loaded() &&
      resource->ContentsValid()) {
    output_resource->set_cached_result(partition->mutable_result());
    result = RewriteSingle(resource, output_resource);
  }
  return result;
}

}  // namespace net_instaweb
