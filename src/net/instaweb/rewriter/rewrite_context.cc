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

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/blocking_behavior.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

namespace {

const char kRewriteContextLockPrefix[] = "rc:";

// Two callback classes for completed caches & fetches.  These gaskets
// help RewriteContext, which knows about all the pending inputs,
// trigger the rewrite once the data is available.  There are two
// versions of the callback.

// Callback to wake up the RewriteContext when the partitioning is looked up
// in the cache.  The RewriteContext can then decide whether to queue the
// output-resource for a DOM update, or re-initiate the Rewrite, depending
// on the metadata returned.
class OutputCacheCallback : public CacheInterface::Callback {
 public:
  explicit OutputCacheCallback(RewriteContext* rc) : rewrite_context_(rc) {}
  virtual ~OutputCacheCallback() {}

  virtual void Done(CacheInterface::KeyState state) {
    rewrite_context_->OutputCacheDone(state, value());
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
};

// Callback to wake up the RewriteContext when an input resource is fetched.
// Once all the resources are fetched (and preceding RewriteContexts completed)
// the Rewrite can proceed.
class ResourceFetchCallback : public Resource::AsyncCallback {
 public:
  ResourceFetchCallback(RewriteContext* rc, const ResourcePtr& resource,
                        int slot_index)
      : Resource::AsyncCallback(resource),
        rewrite_context_(rc),
        slot_index_(slot_index) {}
  virtual ~ResourceFetchCallback() {}

  virtual void Done(bool success) {
    rewrite_context_->ResourceFetchDone(success, resource(), slot_index_);
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
  int slot_index_;
};

}  // namespace

RewriteContext::RewriteContext(RewriteDriver* driver,
                               ResourceContext* resource_context)
  : driver_(driver),
    resource_manager_(driver->resource_manager()),
    started_(false),
    outstanding_fetches_(0),
    resource_context_(resource_context),
    block_(kNeverBlock) {
  // TODO(jmarantz): if this duplication proves expensive, then do this
  // lazily.  We don't need our own copy of the RewriteOptions until the
  // RewriteDriver is detached.  for now just do the simple thing and
  // copy on creation, or ref-count them.
  options_.CopyFrom(*driver_->options());
}

RewriteContext::~RewriteContext() {
}

void RewriteContext::AddSlot(const ResourceSlotPtr& slot) {
  CHECK(!started_);
  slots_.push_back(slot);
}

// Initiate a Rewrite, returning false if we decide not to rewrite, say, due
// to lock failure.
void RewriteContext::Start() {
  CHECK(!started_);
  started_ = true;

  // The best-case scenario for a Rewrite is that we have already done
  // it, and just need to look up in our metadata cache what the final
  // rewritten URL is.  In the simplest scenario, we are doing a
  // simple URL substitution.  In a more complex example, we have M
  // css files that get reduced to N combinations.  The
  // OutputPartitions held in the cache tells us that, and we don't
  // need to get any data about the resources that need to be
  // rewritten.  But in either case, we only need one cache lookup.
  //
  // Note that the output_key_name is not necessarily the same as the
  // name of the output.
  // Write partition to metadata cache.
  StringVector urls;
  for (int i = 0, n = num_slots(); i < n; ++i) {
    urls.push_back(slot(i)->resource()->url());
  }
  encoder()->Encode(urls, resource_context_.get(), &partition_key_);
  CacheInterface* metadata_cache = resource_manager_->metadata_cache();

  // When the cache lookup is finished, OutputCacheDone will be called.
  metadata_cache->Get(partition_key_, new OutputCacheCallback(this));
}

void RewriteContext::OutputCacheDone(CacheInterface::KeyState state,
                                     SharedString* value) {
  if (state == CacheInterface::kAvailable) {
    // If the output cache lookup came as a HIT in after the deadline, that
    // means that (a) we can't use the result and (b) we don't need
    // to re-initiate the rewrite since it was in fact in cache.  Hopefully
    // the cache system will respond to HIT by making the next HIT faster
    // so it meets our deadline.  In either case we will track with stats.
    //
    if (driver_ == NULL) {
      resource_manager_->cached_output_missed_deadline()->Add(1);
    } else {
      resource_manager_->cached_output_hits()->Add(1);
    }

    // We've got a hit on the output metadata; the contents should
    // be a protobuf.  Try to parse it.
    const GoogleString* val_str = value->get();
    ArrayInputStream input(val_str->data(), val_str->size());
    OutputPartitions partitions;
    if (partitions.ParseFromZeroCopyStream(&input)) {
      RenderPartitions(partitions);
      // CAREFUL ABOUT LOCKING SEMANTICS HERE...
      if (driver_ == NULL) {
        delete this;
      }
    } else {
      state = CacheInterface::kNotFound;
      // TODO(jmarantz): count cache corruptions in a stat?
    }

  } else {
    resource_manager_->cached_output_misses()->Add(1);
  }


  // If the cache gave a miss, or yielded unparsable data, then acquire a lock
  // and start fetching the input resources.
  if (state != CacheInterface::kAvailable) {
    // NOTE: This locks based on hash's so if you use a MockHasher, you may
    // only rewrite a single resource at a time (e.g. no rewriting resources
    // inside resources, see css_image_rewriter_test.cc for examples.)
    //
    // TODO(jmarantz): In the multi-resource rewriters that can
    // generate more than one partition, we create a lock based on the
    // entire set of input URLs, plus a lock for each individual
    // output.  However, in single-resource rewriters, we really only
    // need one of these locks.  So figure out which one we'll go with
    // and use that.
    GoogleString lock_name = StrCat(kRewriteContextLockPrefix, partition_key_);
    if (resource_manager_->LockForCreation(lock_name, block_, &lock_)) {
      for (int i = 0, n = slots_.size(); i < n; ++i) {
        const ResourceSlotPtr& slot = slots_[i];
        ResourcePtr resource(slot->resource());
        if (!(resource->loaded() && resource->ContentsValid())) {
          ResourceFetchCallback* callback =
              new ResourceFetchCallback(this, resource, i);
          ++outstanding_fetches_;
          resource_manager_->ReadAsync(callback);

          // TODO(jmarantz): as currently coded this will not work with Apache,
          // as we don't do these async fetches using the threaded fetcher.
          // Those details need to be sorted before we test async rewrites
          // with Apache.
        }
      }
    } else {
      // TODO(jmarantz): bump stat for abandoned rewrites due to lock
      // contention.
    }
  }
}

void RewriteContext::ResourceFetchDone(
    bool success, const ResourcePtr& resource, int slot_index) {
  // LOCK outstanding_fetches_
  CHECK_LT(0, outstanding_fetches_);
  --outstanding_fetches_;
  bool finished = (outstanding_fetches_ == 0);
  // UNLOCK outstanding_fetches_

  if (success) {
    ResourceSlotPtr slot(slots_[slot_index]);

    // For now, we cannot handle if someone updated our slot before us.
    DCHECK(slot.get() != NULL);
    DCHECK_EQ(resource.get(), slot->resource().get());
  }
  if (finished) {
    Finish();
  }
}

void RewriteContext::Finish() {
  // TODO(jmarantz): handle the case where the slots didn't get filled in
  // due to a fetch failure.

  OutputPartitions partitions;
  if (PartitionAndRewrite(&partitions)) {
    CacheInterface* metadata_cache = resource_manager_->metadata_cache();
    SharedString buf;
    {
      StringOutputStream sstream(buf.get());
      partitions.SerializeToZeroCopyStream(&sstream);
      // destructor of sstream prepares *buf.get()
    }
    metadata_cache->Put(partition_key_, &buf);
  }
  lock_.reset();

  RenderPartitions(partitions);
}

void RewriteContext::RenderPartitions(const OutputPartitions& partitions) {
  // LOCK driver
  if (driver_ != NULL) {
    for (int i = 0, n = partitions.partition_size(); i < n; ++i) {
      const OutputPartition& partition = partitions.partition(i);
      const CachedResult& cached_result = partition.result();
      const ContentType* content_type = NameExtensionToContentType(
          StrCat(".", cached_result.extension()));
      if ((content_type != NULL) && FreshenAndCheckExpiration(cached_result)) {
        ResourcePtr output_resource(new UrlInputResource(
            resource_manager(), options(), content_type, cached_result.url()));
        Render(partition, output_resource);
      } else {
        // TODO(jmarantz): bump a failure-due-to-corrupt-cache statistic
      }
    }
  }
  // UNLOCK driver
}

bool RewriteContext::FreshenAndCheckExpiration(const CachedResult& group) {
  // TODO(jmarantz): implement.
  return true;
}

const UrlSegmentEncoder* RewriteContext::encoder() const {
  return &default_encoder_;
}

bool RewriteContext::ComputeOnTheFly() const {
  return false;
}

void RewriteContext::RenderAndDetach() {
  // LOCK driver_
  if (outstanding_fetches_ == 0) {
    for (int i = 0, n = render_slots_.size(); i < n; ++i) {
      render_slots_[i]->Render();
    }
    delete this;
  } else {
    // TODO(jmarantz): Add unit-test that covers this branch.
    driver_ = NULL;
  }
  // UNLOCK driver_
}

}  // namespace net_instaweb
