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
//
// Note: when making changes to this file, a very good sanity-check to run,
// once tests pass, is:
//
// valgrind --leak-check=full ..../src/out/Debug/pagespeed_automatic_test
//     "--gtest_filter=RewriteContextTest*"

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include <cstddef>                     // for size_t
#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/request_headers.h"
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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class RewriteFilter;

const char kRewriteContextLockPrefix[] = "rc:";

// Two callback classes for completed caches & fetches.  These gaskets
// help RewriteContext, which knows about all the pending inputs,
// trigger the rewrite once the data is available.  There are two
// versions of the callback.

// Callback to wake up the RewriteContext when the partitioning is looked up
// in the cache.  The RewriteContext can then decide whether to queue the
// output-resource for a DOM update, or re-initiate the Rewrite, depending
// on the metadata returned.
class RewriteContext::OutputCacheCallback : public CacheInterface::Callback {
 public:
  explicit OutputCacheCallback(RewriteContext* rc) : rewrite_context_(rc) {}
  virtual ~OutputCacheCallback() {}
  virtual void Done(CacheInterface::KeyState state) {
    ResourceManager* resource_manager = rewrite_context_->Manager();
    resource_manager->AddRewriteTask(
        new MemberFunction2<RewriteContext, CacheInterface::KeyState,
            SharedString>(&RewriteContext::OutputCacheDone, rewrite_context_,
                          state, *value()));
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
};

// Common code for invoking RewriteContext::ResourceFetchDone for use
// in ResourceFetchCallback and ResourceReconstructCallback.
class RewriteContext::ResourceCallbackUtils {
 public:
  ResourceCallbackUtils(RewriteContext* rc, const ResourcePtr& resource,
                        int slot_index)
      : resource_(resource),
        rewrite_context_(rc),
        slot_index_(slot_index) {
  }

  void Done(bool success) {
    ResourceManager* resource_manager = rewrite_context_->Manager();
    resource_manager->AddRewriteTask(
        new MemberFunction3<RewriteContext, bool, ResourcePtr, int>(
            &RewriteContext::ResourceFetchDone, rewrite_context_,
            success, resource_, slot_index_));
  }

 private:
  ResourcePtr resource_;
  RewriteContext* rewrite_context_;
  int slot_index_;
};

// Callback when reading a resource from the network.
class RewriteContext::ResourceFetchCallback : public Resource::AsyncCallback {
 public:
  ResourceFetchCallback(RewriteContext* rc, const ResourcePtr& r,
                        int slot_index)
      : Resource::AsyncCallback(r),
        delegate_(rc, r, slot_index) {
  }

  virtual ~ResourceFetchCallback() {}
  virtual void Done(bool success) {
    delegate_.Done(success);
    delete this;
  }

  virtual bool EnableThreaded() const { return true; }

 private:
  ResourceCallbackUtils delegate_;
};

// Callback used when we need to reconstruct a resource we made to satisfy
// a fetch (due to rewrites being nested inside each other).
class RewriteContext::ResourceReconstructCallback :
    public UrlAsyncFetcher::Callback {
 public:
  // Takes ownership of the driver (e.g. will call Cleanup)
  ResourceReconstructCallback(RewriteDriver* driver, RewriteContext* rc,
                              const ResourcePtr& resource, int slot_index)
      : driver_(driver), delegate_(rc, resource, slot_index) {
  }

  virtual ~ResourceReconstructCallback() {
  }

  virtual void Done(bool success) {
    delegate_.Done(success);
    driver_->Cleanup();
    delete this;
  }

  const RequestHeaders& request_headers() const { return request_headers_; }
  ResponseHeaders* response_headers() { return &response_headers_; }
  Writer* writer() { return &writer_; }

 private:
  RewriteDriver* driver_;
  ResourceCallbackUtils delegate_;

  // We ignore the output here as it's also put into the resource itself.
  NullWriter writer_;
  ResponseHeaders response_headers_;
  RequestHeaders request_headers_;
};

// This class encodes a few data members used for responding to
// resource-requests when the output_resource is not in cache.
class RewriteContext::FetchContext {
 public:
  FetchContext(RewriteContext* rewrite_context,
               Writer* writer,
               ResponseHeaders* response_headers,
               UrlAsyncFetcher::Callback* callback,
               const OutputResourcePtr& output_resource,
               MessageHandler* handler)
      : rewrite_context_(rewrite_context),
        writer_(writer),
        response_headers_(response_headers),
        callback_(callback),
        output_resource_(output_resource),
        handler_(handler),
        success_(false) {
  }

  // Note that the callback is called from the RewriteThread.
  void FetchDone() {
    GoogleString output;
    bool ok = false;
    if (success_) {
      // TODO(sligocki): It might be worth streaming this.
      response_headers_->CopyFrom(*(output_resource_->response_headers()));
      ok = writer_->Write(output_resource_->contents(), handler_);
    } else {
      // TODO(jmarantz): implement this:
      // CacheRewriteFailure();

      // Rewrite failed. If we have a single original, write it out instead.
      if (rewrite_context_->num_slots() == 1) {
        ResourcePtr input_resource(rewrite_context_->slot(0)->resource());
        if (input_resource.get() != NULL && input_resource->ContentsValid()) {
          response_headers_->CopyFrom(*input_resource->response_headers());
          ok = writer_->Write(input_resource->contents(), handler_);
        } else {
          GoogleString url = input_resource.get()->url();
          handler_->Error(
              output_resource_->name().as_string().c_str(), 0,
              "Resource based on %s but cannot access the original",
              url.c_str());
        }
      }
    }

    callback_->Done(ok);
  }

  void set_success(bool success) { success_ = success; }
  OutputResourcePtr output_resource() { return output_resource_; }

  RewriteContext* rewrite_context_;
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
  : started_(false),
    outstanding_fetches_(0),
    outstanding_rewrites_(0),
    resource_context_(resource_context),
    num_pending_nested_(0),
    parent_(parent),
    driver_(driver),
    num_predecessors_(0),
    chained_(false),
    rewrite_done_(false),
    ok_to_write_output_partitions_(true) {
  partitions_.reset(new OutputPartitions);
}

RewriteContext::~RewriteContext() {
  DCHECK_EQ(0, num_predecessors_);
  DCHECK_EQ(0, outstanding_fetches_);
  DCHECK(successors_.empty());
  STLDeleteElements(&nested_);
}

int RewriteContext::num_output_partitions() const {
  return partitions_->partition_size();
}

const CachedResult* RewriteContext::output_partition(int i) const {
  return &partitions_->partition(i);
}

CachedResult* RewriteContext::output_partition(int i) {
  return partitions_->mutable_partition(i);
}

void RewriteContext::AddSlot(const ResourceSlotPtr& slot) {
  CHECK(!started_);

  // TODO(jmarantz): eliminate this transitional code to allow JavascriptFilter
  // to straddle the old rewrite flow and the new async flow.
  if (slot.get() == NULL) {
    return;
  }

  slots_.push_back(slot);
  render_slots_.push_back(false);

  RewriteContext* predecessor = slot->LastContext();
  if (predecessor != NULL) {
    // Note that we don't check for duplicate connections between this and
    // predecessor.  They'll all get counted.
    DCHECK(!predecessor->started_);
    predecessor->successors_.push_back(this);
    ++num_predecessors_;
    chained_ = true;
  }
  slot->AddContext(this);
}

void RewriteContext::RemoveLastSlot() {
  int index = num_slots() - 1;
  slot(index)->DetachContext(this);
  RewriteContext* predecessor = slot(index)->LastContext();
  if (predecessor) {
    predecessor->successors_.erase(
        std::find(predecessor->successors_.begin(),
                  predecessor->successors_.end(), this));
    --num_predecessors_;
  }

  slots_.erase(slots_.begin() + index);
  render_slots_.erase(render_slots_.begin() + index);
}

void RewriteContext::Initiate() {
  CHECK(!started_);
  DCHECK(num_predecessors_ == 0);
  Manager()->AddRewriteTask(new MemberFunction0<RewriteContext>(
      &RewriteContext::Start, this));
}

// Initiate a Rewrite if it's ready to be started.  A Rewrite would not
// be startable if was operating on a slot that was already associated
// with another Rewrite.  We would wait for all the preceding rewrites
// to complete before starting this one.
void RewriteContext::Start() {
  DCHECK(!started_);
  DCHECK(num_predecessors_ == 0);
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
  CacheInterface* metadata_cache = Manager()->metadata_cache();
  SetPartitionKey();

  // When the cache lookup is finished, OutputCacheDone will be called.
  metadata_cache->Get(partition_key_, new OutputCacheCallback(this));
}

void RewriteContext::SetPartitionKey() {
  partition_key_ = CacheKey();
  StrAppend(&partition_key_, ":", id());
}

// Check if this mapping from input to output URLs is still valid.
bool RewriteContext::IsCachedResultValid(const CachedResult& partition) {
  for (int j = 0, m = partition.input_size(); j < m; ++j) {
    if (!IsInputValid(partition.input(j))) {
      return false;
    }
  }
  return true;
}

bool RewriteContext::IsOtherDependencyValid(
    const OutputPartitions* partitions) {
  for (int j = 0, m = partitions->other_dependency_size(); j < m; ++j) {
    if (!IsInputValid(partitions->other_dependency(j))) {
      return false;
    }
  }
  return true;
}

void RewriteContext::AddRecheckDependency() {
  int64 now_ms = Manager()->timer()->NowMs();
  InputInfo* force_recheck = partitions_->add_other_dependency();
  force_recheck->set_type(InputInfo::CACHED);
  force_recheck->set_expiration_time_ms(
      now_ms + ResponseHeaders::kImplicitCacheTtlMs);
}

bool RewriteContext::IsInputValid(const InputInfo& input_info) {
  switch (input_info.type()) {
    case InputInfo::CACHED: {
      // It is invalid if cacheable inputs have expired or ...
      DCHECK(input_info.has_expiration_time_ms());
      if (!input_info.has_expiration_time_ms()) {
        return false;
      }
      int64 now_ms = Manager()->timer()->NowMs();
      return (now_ms <= input_info.expiration_time_ms());
      break;
    }
    case InputInfo::FILE_BASED: {
      // ... if file-based inputs have changed.
      DCHECK(input_info.has_last_modified_time_ms() &&
             input_info.has_filename());
      if (!input_info.has_last_modified_time_ms() ||
          !input_info.has_filename()) {
        return false;
      }
      int64 mtime_sec;
      Manager()->file_system()->Mtime(input_info.filename(), &mtime_sec,
                                      Manager()->message_handler());
      return (mtime_sec * Timer::kSecondMs ==
                input_info.last_modified_time_ms());
      break;
    }
    case InputInfo::ALWAYS_VALID:
      return true;
  }

  DCHECK(false) << "Corrupt InputInfo object !?";
  return false;
}

void RewriteContext::OutputCacheDone(CacheInterface::KeyState state,
                                     SharedString value) {
  DCHECK_LE(0, outstanding_fetches_);
  DCHECK_EQ(static_cast<size_t>(0), outputs_.size());
  if (state == CacheInterface::kAvailable) {
    // We've got a hit on the output metadata; the contents should
    // be a protobuf.  Try to parse it.
    const GoogleString* val_str = value.get();
    ArrayInputStream input(val_str->data(), val_str->size());
    if (partitions_->ParseFromZeroCopyStream(&input) &&
        IsOtherDependencyValid(partitions_.get())) {
      for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
        const CachedResult& partition = partitions_->partition(i);
        OutputResourcePtr output_resource;
        const ContentType* content_type = NameExtensionToContentType(
            StrCat(".", partition.extension()));

        // TODO(sligocki): Move this into FreshenAndCheckExpiration or delete
        // that (currently empty) method.
        if (!IsCachedResultValid(partition)) {
          // If a single output resource is invalid, we update them all.
          state = CacheInterface::kNotFound;
          outputs_.clear();
          break;
        }

        if (partition.optimizable() &&
            CreateOutputResourceForCachedOutput(
                partition.url(), content_type, &output_resource) &&
            FreshenAndCheckExpiration(partition)) {
          outputs_.push_back(output_resource);
          RenderPartitionOnDetach(i);
        } else {
          outputs_.push_back(OutputResourcePtr(NULL));
        }
      }
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
    rewrite_done_ = true;
    ok_to_write_output_partitions_ = false;  // partitions were read succesfully
    Finalize();
  } else {
    partitions_->Clear();
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
        ++outstanding_fetches_;

        // In case of fetches, we may need to handle rewrites nested inside
        // each other; so we want to pass them on to other rewrite tasks
        // rather than try to fetch them over HTTP.
        bool handled_internally = false;
        if (fetch_.get() != NULL) {
          RewriteFilter* filter = NULL;
          OutputResourcePtr output_resource(
              Driver()->DecodeOutputResource(resource->url(), &filter));
          if (output_resource.get() != NULL) {
            RewriteDriver* nested_driver = Driver()->Clone();
            // Re-grab the filter so we get one that's bound to the new
            // RewriteDriver.
            // TODO(morlovich): How inefficient. Maybe I should have
            // DecodeOutputResource return the filter enum as well?
            output_resource =
                nested_driver->DecodeOutputResource(resource->url(), &filter);
            DCHECK(output_resource.get() != NULL);
            if (output_resource.get() != NULL) {
              handled_internally = true;
              ResourcePtr updated_resource(output_resource);
              slot->SetResource(updated_resource);
              ResourceReconstructCallback* callback =
                  new ResourceReconstructCallback(
                      nested_driver, this, updated_resource, i);
              nested_driver->FetchOutputResource(
                  output_resource, filter,
                  callback->request_headers(),
                  callback->response_headers(),
                  callback->writer(),
                  callback);
            } else {
              Manager()->ReleaseRewriteDriver(nested_driver);
            }
          }
        }

        if (!handled_internally) {
          Manager()->ReadAsync(new ResourceFetchCallback(this, resource, i));
        }
      }
    }

    --num_predecessors_;
  } else {
    // TODO(jmarantz): bump stat for abandoned rewrites due to lock
    // contention.
  }

  Activate();  // TODO(jmarantz): remove.
}

void RewriteContext::ResourceFetchDone(
    bool success, ResourcePtr resource, int slot_index) {
  CHECK_LT(0, outstanding_fetches_);
  --outstanding_fetches_;

  if (success) {
    ResourceSlotPtr slot(slots_[slot_index]);

    // For now, we cannot handle if someone updated our slot before us.
    DCHECK(slot.get() != NULL);
    DCHECK_EQ(resource.get(), slot->resource().get());
  }
  Activate();
}

bool RewriteContext::ReadyToRewrite() const {
  DCHECK(!rewrite_done_);
  bool ready = ((outstanding_fetches_ == 0) && (num_predecessors_ == 0));
  return ready;
}

void RewriteContext::Activate() {
  if (ReadyToRewrite()) {
    if (fetch_.get() == NULL) {
      DCHECK(started_);
      StartRewrite();
    } else {
      FinishFetch();
    }
  }
}

void RewriteContext::StartRewrite() {
  if (!Partition(partitions_.get(), &outputs_)) {
    partitions_->clear_partition();
    outputs_.clear();
  }

  outstanding_rewrites_ = partitions_->partition_size();
  if (outstanding_rewrites_ == 0) {
    // The partitioning succeeded, but yielded zero rewrites.  Write out the
    // empty partition table and let any successor Rewrites run.
    rewrite_done_ = true;

    // TODO(morlovich): The filters really should be doing this themselves,
    // since there may be partial failures in cases of multiple inputs which
    // we do not see here.
    AddRecheckDependency();
    WritePartition();
  } else {
    // We will let the Rewrites complete prior to writing the
    // OutputPartitions, which contain not just the partition table
    // but the content-hashes for the rewritten content.  So we must
    // rewrite before calling WritePartitions.
    CHECK_EQ(outstanding_rewrites_, static_cast<int>(outputs_.size()));
    for (int i = 0, n = outstanding_rewrites_; i < n; ++i) {
      Rewrite(i, partitions_->mutable_partition(i), outputs_[i]);
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
  } else {
    // TODO(jmarantz): if our rewrite failed due to lock contention or
    // being too busy, then cancel all successors.
  }
  lock_.reset();
  if (parent_ != NULL) {
    DCHECK(driver_ == NULL);
    Propagate(true);
    parent_->NestedRewriteDone(this);
  } else {
    // The RewriteDriver is waiting for this to complete.  Defer to the
    // RewriteDriver to schedule the Rendering of this context on the main
    // thread.
    CHECK(driver_ != NULL);
    driver_->RewriteComplete(this);
  }
}

void RewriteContext::AddNestedContext(RewriteContext* context) {
  ++num_pending_nested_;
  nested_.push_back(context);
  context->parent_ = this;
}

void RewriteContext::StartNestedTasks() {
  for (int i = 0, n = nested_.size(); i < n; ++i) {
    if (!nested_[i]->chained()) {
      nested_[i]->Start();
      DCHECK_EQ(n, static_cast<int>(nested_.size()))
          << "Cannot add new nested tasks once the nested tasks have started";
    }
  }
}

void RewriteContext::NestedRewriteDone(const RewriteContext* context) {
  // Record any external dependencies we have.
  // TODO(morlovich): Eliminate duplicates?
  for (int p = 0; p < context->num_output_partitions(); ++p) {
    const CachedResult* nested_result = context->output_partition(p);
    for (int i = 0; i < nested_result->input_size(); ++i) {
      InputInfo* dep = partitions_->add_other_dependency();
      dep->CopyFrom(nested_result->input(i));
      // The input index here is with respect to the nested context's inputs,
      // so would not be interpretable at top-level, and we don't use it for
      // other_dependency entries anyway, so be both defensive and frugal
      // and don't write it out.
      dep->clear_index();
    }
  }

  for (int p = 0; p < context->partitions_->other_dependency_size(); ++p) {
    InputInfo* dep = partitions_->add_other_dependency();
    dep->CopyFrom(context->partitions_->other_dependency(p));
  }

  DCHECK_LT(0, num_pending_nested_);
  --num_pending_nested_;
  if (num_pending_nested_ == 0) {
    DCHECK(!rewrite_done_);
    Harvest();
  }
}

void RewriteContext::RewriteDone(
    RewriteSingleResourceFilter::RewriteResult result,
    int partition_index) {
  if (result == RewriteSingleResourceFilter::kTooBusy) {
    ok_to_write_output_partitions_ = false;
  } else {
    CachedResult* partition =
        partitions_->mutable_partition(partition_index);
    bool optimizable = (result == RewriteSingleResourceFilter::kRewriteOk);
    partition->set_optimizable(optimizable);
    if (optimizable && (fetch_.get() == NULL)) {
      // TODO(morlovich): currently in async mode, we tie rendering of slot
      // to the optimizable bit, making it impossible to do per-slot mutation
      // that doesn't involve the output URL.
      RenderPartitionOnDetach(partition_index);
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
}

void RewriteContext::Propagate(bool render_slots) {
  DCHECK(rewrite_done_ && (num_pending_nested_ == 0));
  if (rewrite_done_ && (num_pending_nested_ == 0)) {
    if (render_slots) {
      Render();
    }
    CHECK_EQ(num_output_partitions(), static_cast<int>(outputs_.size()));
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      for (int i = 0, n = partition->input_size(); i < n; ++i) {
        int slot_index = partition->input(i).index();
        if (render_slots_[slot_index]) {
          ResourcePtr resource(outputs_[p]);
          slots_[slot_index]->SetResource(resource);
          if (render_slots) {
            slots_[slot_index]->Render();
          }
        }
      }
    }
  }

  RunSuccessors();
}

void RewriteContext::Finalize() {
  DCHECK(rewrite_done_);
  if (num_pending_nested_ == 0) {
    if (fetch_.get() != NULL) {
      fetch_->FetchDone();
    } else {
      WritePartition();
    }
  }
}

void RewriteContext::RenderPartitionOnDetach(int rewrite_index) {
  CachedResult* partition = output_partition(rewrite_index);
  for (int i = 0; i < partition->input_size(); ++i) {
    int slot_index = partition->input(i).index();
    slot(slot_index)->set_was_optimized();
    render_slots_[slot_index] = true;
  }
}

void RewriteContext::RunSuccessors() {
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    slot(i)->DetachContext(this);
  }

  for (int i = 0, n = successors_.size(); i < n; ++i) {
    RewriteContext* successor = successors_[i];
    if (--successor->num_predecessors_ == 0) {
      successor->Initiate();
    }
  }
  successors_.clear();
  if (driver_ != NULL) {
    DCHECK(rewrite_done_ && (num_pending_nested_ == 0));
    Manager()->AddRewriteTask(
        new MemberFunction1<RewriteDriver, RewriteContext*>(
            &RewriteDriver::DeleteRewriteContext, driver_, this));
  }
}

void RewriteContext::FinishFetch() {
  // Make a fake partition that has all the inputs, since we are
  // performing the rewrite for only one output resource.
  CachedResult* partition = partitions_->add_partition();
  bool ok_to_rewrite = true;
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    ResourcePtr resource(slot(i)->resource());
    if (resource->loaded() && resource->ContentsValid()) {
      resource->AddInputInfoToPartition(i, partition);
    } else {
      ok_to_rewrite = false;
      break;
    }
  }
  OutputResourcePtr output(fetch_->output_resource());
  ++outstanding_rewrites_;
  if (ok_to_rewrite) {
    Rewrite(0, partition, output);
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

GoogleString RewriteContext::CacheKey() const {
  GoogleString key;
  StringVector urls;
  for (int i = 0, n = num_slots(); i < n; ++i) {
    ResourcePtr resource(slot(i)->resource());
    urls.push_back(resource->url());
  }
  encoder()->Encode(urls, resource_context_.get(), &key);
  return key;
}

bool RewriteContext::Fetch(
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
  RewriteDriver* driver = Driver();
  driver->InitiateFetch(this);
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
    SetPartitionKey();
    fetch_.reset(
        new FetchContext(this, response_writer, response_headers, callback,
                         output_resource, message_handler));
    Manager()->AddRewriteTask(new MemberFunction0<RewriteContext>(
        &RewriteContext::StartFetch, this));
    ret = true;
  }
  return ret;
}

void RewriteContext::StartFetch() {
  FetchInputs(kMayBlock);
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
