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
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/google_url.h"  // for GoogleUrl
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"  // for GoogleString, NULL
#include "net/instaweb/util/public/string_util.h"  // for StringVector, etc
#include "net/instaweb/util/public/url_segment_encoder.h"

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
    GoogleUrl gurl(resource->url());
    UrlPartnership partnership(Options(), gurl);
    ResourceNamer full_name;
    if (resource->loaded() &&
        resource->ContentsValid() &&
        partnership.AddUrl(resource->url(), Manager()->message_handler())) {
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
          Manager(), gurl.AllExceptLeaf(), full_name, content_type,
          Options(), kind()));
      output_resource->set_written_using_rewrite_context_flow(true);
      OutputPartition* partition = partitions->add_partition();
      partition->add_input(0);
      output_resource->set_cached_result(partition->mutable_result());
      outputs->push_back(output_resource);
    }
  }
  return ret;
}

void SingleRewriteContext::Rewrite(OutputPartition* partition,
                                   const OutputResourcePtr& output_resource) {
  ResourcePtr resource(slot(0)->resource());
  CHECK(resource.get() != NULL);
  CHECK(resource->loaded());
  CHECK(resource->ContentsValid());
  output_resource->set_cached_result(partition->mutable_result());
  RewriteSingle(resource, output_resource);
}

}  // namespace net_instaweb
