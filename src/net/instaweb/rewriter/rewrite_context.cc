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

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/blocking_behavior.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;

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

// This class encodes a few data members used for responding to
// resource-requests when the output_resource is not in cache.
class RewriteContext::FetchContext {
 public:
  FetchContext(Writer* writer,
               ResponseHeaders* response_headers,
               UrlAsyncFetcher::Callback* callback,
               const OutputResourcePtr& output_resource,
               MessageHandler* handler)
      : writer_(writer),
        response_headers_(response_headers),
        callback_(callback),
        output_resource_(output_resource),
        handler_(handler),
        success_(false) {
  }

  void FetchDone() {
    GoogleString output;
    if (success_) {
      // TODO(sligocki): It might be worth streaming this.
      response_headers_->CopyFrom(*(output_resource_->metadata()));
      writer_->Write(output_resource_->contents(), handler_);
    } else {
      // TODO(jmarantz): implement this:
      // CacheRewriteFailure();
      // Rewrite failed. If we have the original, write it out instead.
      // if (input_resource_->ContentsValid()) {
      //  WriteFromResource(input_resource_.get());
      //    success = true;
      // }   else {
      //  // If not, log the failure.
      //  GoogleString url = input_resource_.get()->url();
      //  handler_->Error(
      //      output_resource_->name().as_string().c_str(), 0,
      //      "Resource based on %s but cannot find the original", url.c_str());
      // }
      //
      // TODO(jmarantz): morlovich points out: Looks like you'll need the
      // vector of inputs for both cases (and multiple inputs can't do
      // the passthrough fallback)
    }

    callback_->Done(success_);
  }

  void set_success(bool success) { success_ = success; }
  OutputResourcePtr output_resource() { return output_resource_; }

  Writer* writer_;
  ResponseHeaders* response_headers_;
  UrlAsyncFetcher::Callback* callback_;
  OutputResourcePtr output_resource_;
  MessageHandler* handler_;
  bool success_;
};

RewriteContext::RewriteContext(RewriteDriver* driver,
                               RewriteContext* parent,
                               ResourceContext* resource_context)
  : attached_(true),
    written_(false),
    started_(false),
    outstanding_fetches_(0),
    outstanding_rewrites_(0),
    resource_context_(resource_context),
    num_pending_nested_(0),
    parent_(parent),
    driver_(driver),
    num_predecessors_(0),
    cache_lookup_active_(false),
    rewrite_done_(false),
    ok_to_write_output_partitions_(true) {
  partitions_.reset(new OutputPartitions);
}

RewriteContext::~RewriteContext() {
  DCHECK_EQ(0, num_predecessors_);
  DCHECK_EQ(0, outstanding_fetches_);
  DCHECK(successors_.empty());
}

void RewriteContext::AddSlot(const ResourceSlotPtr& slot) {
  CHECK(!started_);

  // TODO(jmarantz): eliminate this transitional code to allow JavascriptFilter
  // to straddle the old rewrite flow and the new async flow.
  if (slot.get() == NULL) {
    return;
  }

  slots_.push_back(slot);

  RewriteContext* predecessor = slot->LastContext();
  if (predecessor != NULL) {
    // Note that we don't check for duplicate connections between this and
    // predecessor.  They'll all get counted.
    predecessor->successors_.push_back(this);
    ++num_predecessors_;
  }
  slot->AddContext(this);
}

// Initiate a Rewrite, returning false if we decide not to rewrite, say, due
// to lock failure.
void RewriteContext::Start() {
  CHECK(!started_);
  if (num_predecessors_ == 0) {
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
    CacheInterface* metadata_cache = Manager()->metadata_cache();

    // When the cache lookup is finished, OutputCacheDone will be called.
    cache_lookup_active_ = true;
    metadata_cache->Get(partition_key_, new OutputCacheCallback(this));
  }
}

void RewriteContext::OutputCacheDone(CacheInterface::KeyState state,
                                     SharedString* value) {
  cache_lookup_active_ = false;
  if (state == CacheInterface::kAvailable) {
    // If the output cache lookup came as a HIT in after the deadline, that
    // means that (a) we can't use the result and (b) we don't need
    // to re-initiate the rewrite since it was in fact in cache.  Hopefully
    // the cache system will respond to HIT by making the next HIT faster
    // so it meets our deadline.  In either case we will track with stats.
    //
    if (attached_) {
      Manager()->cached_output_hits()->Add(1);
    } else {
      Manager()->cached_output_missed_deadline()->Add(1);
    }

    // We've got a hit on the output metadata; the contents should
    // be a protobuf.  Try to parse it.
    const GoogleString* val_str = value->get();
    ArrayInputStream input(val_str->data(), val_str->size());
    if (partitions_->ParseFromZeroCopyStream(&input)) {
      for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
        const OutputPartition& partition = partitions_->partition(i);
        const CachedResult& cached_result = partition.result();
        OutputResourcePtr output_resource;
        const ContentType* content_type = NameExtensionToContentType(
            StrCat(".", cached_result.extension()));
        if (cached_result.optimizable() &&
            CreateOutputResourceForCachedOutput(
            cached_result.url(), content_type, &output_resource) &&
            FreshenAndCheckExpiration(cached_result)) {
          outputs_.push_back(output_resource);
          RenderSlotOnDetach(i);
        } else {
          outputs_.push_back(OutputResourcePtr(NULL));
        }
      }
      rewrite_done_ = true;
    } else {
      state = CacheInterface::kNotFound;
      // TODO(jmarantz): count cache corruptions in a stat?
    }
  } else {
    Manager()->cached_output_misses()->Add(1);
  }

  // If the cache gave a miss, or yielded unparsable data, then acquire a lock
  // and start fetching the input resources.
  if (state == CacheInterface::kAvailable) {
    ok_to_write_output_partitions_ = false;  // partitions were read succesfully
    Finalize();
  } else {
    FetchInputs(kNeverBlock);
  }
}

void RewriteContext::FetchInputs(BlockingBehavior block) {
  // NOTE: This lock is based on hashes so if you use a MockHasher, you may
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

  if (Manager()->LockForCreation(lock_name, block, &lock_)) {
    ++num_predecessors_;

    for (int i = 0, n = slots_.size(); i < n; ++i) {
      const ResourceSlotPtr& slot = slots_[i];
      ResourcePtr resource(slot->resource());
      if (!(resource->loaded() && resource->ContentsValid())) {
        ResourceFetchCallback* callback = new ResourceFetchCallback(
            this, resource, i);
        ++outstanding_fetches_;
        Manager()->ReadAsync(callback);

        // TODO(jmarantz): as currently coded this will not work with Apache,
        // as we don't do these async fetches using the threaded fetcher.
        // Those details need to be sorted before we test async rewrites
        // with Apache.
      }
    }

    --num_predecessors_;
  } else {
    // TODO(jmarantz): bump stat for abandoned rewrites due to lock
    // contention.
  }

  Activate();
}

void RewriteContext::ResourceFetchDone(
    bool success, const ResourcePtr& resource, int slot_index) {
  // LOCK outstanding_fetches_
  CHECK_LT(0, outstanding_fetches_);
  --outstanding_fetches_;
  // UNLOCK outstanding_fetches_

  if (success) {
    ResourceSlotPtr slot(slots_[slot_index]);

    // For now, we cannot handle if someone updated our slot before us.
    DCHECK(slot.get() != NULL);
    DCHECK_EQ(resource.get(), slot->resource().get());
  }
  Activate();
}

bool RewriteContext::ReadyToRewrite() const {
  // LOCK outstanding_fetches_
  bool ready = ((outstanding_fetches_ == 0) && (num_predecessors_ == 0) &&
                !cache_lookup_active_ && !rewrite_done_);
  // UNLOCK outstanding_fetches_
  return ready;
}

void RewriteContext::Activate() {
  if (ReadyToRewrite()) {
    if (fetch_.get() == NULL) {
      if (started_) {
        StartRewrite();
      } else {
        Start();
      }
    } else {
      FinishFetch();
    }
  }
}

void RewriteContext::StartRewrite() {
  if (Partition(partitions_.get(), &outputs_)) {
    outstanding_rewrites_ = partitions_->partition_size();
    if (outstanding_rewrites_ == 0) {
      // The partitioning succeeded, but yielded zero rewrites.  Write out the
      // empty partition table and let any successor Rewrites run.  Note that
      // WritePartitoin can 'delete this'.
      rewrite_done_ = true;
      WritePartition();
    } else {
      // We will let the Rewrites complete prior to writing the
      // OutputPartitions, which contain not just the partition table
      // but the content-hashes for the rewritten content.  So we must
      // rewrite before callling WritePartitions.
      for (int i = 0, n = outstanding_rewrites_; i < n; ++i) {
        Rewrite(partitions_->mutable_partition(i), outputs_[i]);
      }
    }
  }
}

void RewriteContext::WritePartition() {
  if (ok_to_write_output_partitions_) {
    CacheInterface* metadata_cache = Manager()->metadata_cache();
    SharedString buf;
    {
      StringOutputStream sstream(buf.get());
      partitions_->SerializeToZeroCopyStream(&sstream);
      // destructor of sstream prepares *buf.get()
    }
    metadata_cache->Put(partition_key_, &buf);
  }
  lock_.reset();
  if (parent_ != NULL) {
    parent_->NestedRewriteDone();
  }
  RunSuccessors();
  if (!attached_) {
    delete this;
  } else {
    written_ = true;
  }
}

void RewriteContext::AddNestedContext(RewriteContext* context) {
  ++num_pending_nested_;
  nested_.push_back(context);
  context->parent_ = this;
}

void RewriteContext::StartNestedTasks() {
  for (int i = 0, n = nested_.size(); i < n; ++i) {
    nested_[i]->Start();
    DCHECK_EQ(n, static_cast<int>(nested_.size()))
        << "Cannot add new nested tasks once the nested tasks have started";
  }
}

void RewriteContext::NestedRewriteDone() {
  DCHECK_LT(0, num_pending_nested_);
  --num_pending_nested_;
  if (num_pending_nested_ == 0) {
    DCHECK(!rewrite_done_);
    Harvest();
    DCHECK(rewrite_done_);
  }
}

void RewriteContext::RewriteDone(
    RewriteSingleResourceFilter::RewriteResult result,
    int rewrite_index) {
  if (result == RewriteSingleResourceFilter::kTooBusy) {
    ok_to_write_output_partitions_ = false;
  } else {
    OutputPartition* partition = partitions_->mutable_partition(rewrite_index);
    bool optimizable = (result == RewriteSingleResourceFilter::kRewriteOk);
    partition->mutable_result()->set_optimizable(optimizable);
    if (optimizable && (fetch_.get() == NULL)) {
      // TODO(jmarantz): In the async world we consider any mutation
      // to the DOM an 'optimization', even if the resource is not
      // rewritten.  We should cache that in the OutputPartitions.  I
      // think our current usage of the CachedResult protobuf does not
      // have a representation for that but we could probably invent
      // a convention, like leaving the url() in the protobuf unset.
      RenderSlotOnDetach(rewrite_index);
    }
  }
  --outstanding_rewrites_;
  if (outstanding_rewrites_ == 0) {
    rewrite_done_ = true;
    if (fetch_.get() != NULL) {
      fetch_->set_success((result == RewriteSingleResourceFilter::kRewriteOk));
    }
    Finalize();
  }
}

void RewriteContext::Harvest() {
}

void RewriteContext::Render() {
  if (rewrite_done_ && (num_pending_nested_ == 0)) {
    for (int i = 0, n = render_slots_.size(); i < n; ++i) {
      render_slots_[i]->Render();
    }
  }
}

void RewriteContext::Finalize() {
  DCHECK(rewrite_done_);
  if (num_pending_nested_ == 0) {
    if (fetch_.get() != NULL) {
      fetch_->FetchDone();
      delete this;
    } else {
      WritePartition();
    }
  }
}

void RewriteContext::RenderSlotOnDetach(int rewrite_index) {
  ResourceSlotPtr resource_slot(slot(rewrite_index));
  ResourcePtr resource(outputs_[rewrite_index]);
  resource_slot->SetResource(resource);
  render_slots_.push_back(resource_slot);
}

void RewriteContext::RunSuccessors() {
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    slot(i)->DetachContext(this);
  }

  for (int i = 0, n = successors_.size(); i < n; ++i) {
    RewriteContext* successor = successors_[i];
    --successor->num_predecessors_;
    successor->Activate();
  }
  successors_.clear();
}

void RewriteContext::FinishFetch() {
  // Make a fake partition that has all the inputs, since we are
  // performing the rewrite for only one output resource.
  OutputPartition* partition = partitions_->add_partition();
  bool ok_to_rewrite = true;
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    ResourcePtr resource(slot(i)->resource());
    if (resource->loaded() && resource->ContentsValid()) {
      partition->add_input(i);
    } else {
      ok_to_rewrite = false;
      break;
    }
  }
  OutputResourcePtr output(fetch_->output_resource());
  ++outstanding_rewrites_;
  if (ok_to_rewrite) {
    Rewrite(partition, output);
  } else {
    RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
  }
}

bool RewriteContext::CreateOutputResourceForCachedOutput(
    const StringPiece& url, const ContentType* content_type,
    OutputResourcePtr* output_resource) {
  bool ret = false;
  GoogleUrl gurl(url);
  ResourceNamer namer;
  if (gurl.is_valid() && namer.Decode(gurl.LeafWithQuery())) {
    output_resource->reset(new OutputResource(
        Manager(), gurl.AllExceptLeaf(), namer, content_type,
        Options(), kind()));
    (*output_resource)->set_written_using_rewrite_context_flow(true);
    ret = true;
  }
  return ret;
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

void RewriteContext::Detach() {
  DCHECK(attached_);
  attached_ = false;
  if (written_) {
    delete this;
  }
  // If written_ is false that means that the HTML rewriter's rewrite deadline
  // expired without the rewrites completing.  The Rewrite will continue until
  // it is done, and will delete itself at the end of WriteParitition.
}

bool RewriteContext::Fetch(
    RewriteDriver* driver,
    const OutputResourcePtr& output_resource,
    Writer* response_writer,
    ResponseHeaders* response_headers,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* callback) {
  // Decode the URLs required to execute the rewrite.
  bool ret = false;
  StringVector urls;
  GoogleUrl gurl(output_resource->url());
  GoogleUrl base(gurl.AllExceptLeaf());
  if (encoder()->Decode(output_resource->name(), &urls, resource_context_.get(),
                        message_handler)) {
    for (int i = 0, n = urls.size(); i < n; ++i) {
      GoogleUrl url(base, urls[i]);
      if (!url.is_valid()) {
        return false;
      }
      ResourcePtr resource(driver->CreateInputResource(url));
      if (resource.get() == NULL) {
        // TODO(jmarantz): bump invalid-input-resource count
        return false;
      }
      ResourceSlotPtr slot(new FetchResourceSlot(resource));
      AddSlot(slot);
    }
    fetch_.reset(new FetchContext(response_writer, response_headers, callback,
                                  output_resource, message_handler));
    FetchInputs(kMayBlock);
    ret = true;
  }
  return ret;
}

RewriteDriver* RewriteContext::Driver() {
  RewriteContext* rc;
  for (rc = this; rc->driver_ == NULL; rc = rc->parent_) {
    CHECK(rc != NULL);
  }
  return rc->driver_;
}

ResourceManager* RewriteContext::Manager() {
  return Driver()->resource_manager();
}

const RewriteOptions* RewriteContext::Options() {
  return Driver()->options();
}

}  // namespace net_instaweb
