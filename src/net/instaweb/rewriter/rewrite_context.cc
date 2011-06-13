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
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/content_type.h"
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
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/worker.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;

const char kRewriteContextLockPrefix[] = "rc:";

// Base class for synchronizing fetcher & cache callbacks with the Rewrite
// thread by adding them to the work-queue in the ResourceManager.  This
// class is also responsible for calling the private methods in RewriteContext,
// as it's been authorized to do so via a 'friend' declaration, which does
// not extend to the subclasses.
class RewriteContextTask : public Worker::Closure {
 public:
  explicit RewriteContextTask(RewriteContext* rc) : rewrite_context_(rc) {}
  void Queue() {
    ResourceManager* resource_manager = rewrite_context_->Manager();
    resource_manager->AddRewriteTask(this);
  }
 protected:
  // Provide helper methods for subclasses, because RewriteContextTask
  // has been given 'friend' access to these private methods of RewriteContext.
  void OutputCacheDone(CacheInterface::KeyState state,
                       SharedString* value) {
    rewrite_context_->OutputCacheDone(state, value);
  }
  void ResourceFetchDone(
      bool success, const ResourcePtr& resource, int slot_index) {
    rewrite_context_->ResourceFetchDone(success, resource, slot_index);
  }
  void StartFetch() { rewrite_context_->StartFetch(); }
  void Start() { rewrite_context_->Start(); }
  void RunSuccessors() { rewrite_context_->RunSuccessors(); }

 private:
  RewriteContext* rewrite_context_;
};

namespace {

// Two callback classes for completed caches & fetches.  These gaskets
// help RewriteContext, which knows about all the pending inputs,
// trigger the rewrite once the data is available.  There are two
// versions of the callback.

// Callback to wake up the RewriteContext when the partitioning is looked up
// in the cache.  The RewriteContext can then decide whether to queue the
// output-resource for a DOM update, or re-initiate the Rewrite, depending
// on the metadata returned.
class OutputCacheTask : public RewriteContextTask {
 public:
  explicit OutputCacheTask(RewriteContext* rc)
      : RewriteContextTask(rc),
        callback_(this) {
  }
  virtual ~OutputCacheTask() {}
  virtual void Run() { OutputCacheDone(state_, &value_); }
  class Callback : public CacheInterface::Callback {
   public:
    explicit Callback(OutputCacheTask* task) : task_(task) {}
    virtual ~Callback() {}
    virtual void Done(CacheInterface::KeyState state) {
      task_->state_ = state;
      task_->value_ = *value();
      task_->Queue();
    }

   private:
    OutputCacheTask* task_;
  };
  Callback* callback() { return &callback_; }

  Callback callback_;
  CacheInterface::KeyState state_;
  SharedString value_;
};

// Callback to wake up the RewriteContext when an input resource is fetched.
// Once all the resources are fetched (and preceding RewriteContexts completed)
// the Rewrite can proceed.
class ResourceFetchTask : public RewriteContextTask {
 public:
  ResourceFetchTask(RewriteContext* rc, const ResourcePtr& resource,
                    int slot_index)
      : RewriteContextTask(rc),
        callback_(resource, this),
        slot_index_(slot_index) {}
  virtual ~ResourceFetchTask() {}

  class Callback : public Resource::AsyncCallback {
   public:
    Callback(const ResourcePtr& resource, ResourceFetchTask* task)
        : Resource::AsyncCallback(resource),
          task_(task) {
    }
    virtual ~Callback() {}
    virtual void Done(bool success) {
      task_->success_ = success;
      task_->Queue();
    }

   private:
    ResourceFetchTask* task_;
  };

  Callback* callback() { return &callback_; }
  virtual void Run() {
    ResourceFetchDone(success_, callback_.resource(), slot_index_);
  }

 private:
  Callback callback_;
  int slot_index_;
  bool success_;
};

class StartTask : public RewriteContextTask {
 public:
  explicit StartTask(RewriteContext* rc) : RewriteContextTask(rc) {}
  virtual ~StartTask() {}
  virtual void Run() { Start(); }
};

class FetchTask : public RewriteContextTask {
 public:
  explicit FetchTask(RewriteContext* rc) : RewriteContextTask(rc) {}
  virtual ~FetchTask() {}
  virtual void Run() { StartFetch(); }
};

class SuccessorsTask : public RewriteContextTask {
 public:
  explicit SuccessorsTask(RewriteContext* rc) : RewriteContextTask(rc) {}
  virtual ~SuccessorsTask() {}
  virtual void Run() { RunSuccessors(); }
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

  // Note that the callback is called from the RewriteThread.
  void FetchDone() {
    GoogleString output;
    if (success_) {
      // TODO(sligocki): It might be worth streaming this.
      response_headers_->CopyFrom(*(output_resource_->response_headers()));
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
  : started_(false),
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
  STLDeleteElements(&nested_);
}

int RewriteContext::num_output_partitions() const {
  return partitions_->partition_size();
}

const OutputPartition* RewriteContext::output_partition(int i) const {
  return &partitions_->partition(i);
}

OutputPartition* RewriteContext::output_partition(int i) {
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
  }
  slot->AddContext(this);
}

void RewriteContext::Initiate() {
  CHECK(!started_);
  Manager()->AddRewriteTask(new StartTask(this));
}

// Initiate a Rewrite if it's ready to be started.  A Rewrite would not
// be startable if was operating on a slot that was already associated
// with another Rewrite.  We would wait for all the preceding rewrites
// to complete before starting this one.
void RewriteContext::Start() {
  DCHECK(!started_);
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
      ResourcePtr resource(slot(i)->resource());
      urls.push_back(resource->url());
    }
    encoder()->Encode(urls, resource_context_.get(), &partition_key_);
    StrAppend(&partition_key_, ":", id());
    CacheInterface* metadata_cache = Manager()->metadata_cache();

    // When the cache lookup is finished, OutputCacheDone will be called.
    cache_lookup_active_ = true;
    metadata_cache->Get(partition_key_,
                        (new OutputCacheTask(this))->callback());
  }
}

void RewriteContext::OutputCacheDone(CacheInterface::KeyState state,
                                     SharedString* value) {
  DCHECK_LE(0, outstanding_fetches_);
  DCHECK_EQ(static_cast<size_t>(0), outputs_.size());
  cache_lookup_active_ = false;
  if (state == CacheInterface::kAvailable) {
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
        // TODO(sligocki): cached_results telling us to remember not to
        // rewrite resources are failing here.
        // TODO(sligocki): Move this into FreshenAndCheckExpiration or delete
        // that (currently empty) method.
        if (Manager()->IsCachedResultExpired(cached_result)) {
          // If a single output resource has expired, we update them all.
          state = CacheInterface::kNotFound;
          outputs_.clear();
          break;
        }
        if (cached_result.optimizable() &&
            CreateOutputResourceForCachedOutput(
                cached_result.url(), content_type, &output_resource) &&
            FreshenAndCheckExpiration(cached_result)) {
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
        ResourceFetchTask* task = new ResourceFetchTask(this, resource, i);
        ++outstanding_fetches_;
        Manager()->ReadAsync(task->callback());

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

  Activate();  // TODO(jmarantz): remove.
}

void RewriteContext::ResourceFetchDone(
    bool success, const ResourcePtr& resource, int slot_index) {
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
  bool ready = ((outstanding_fetches_ == 0) && (num_predecessors_ == 0) &&
                !cache_lookup_active_ && !rewrite_done_);
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
  if (!Partition(partitions_.get(), &outputs_)) {
    partitions_->clear_partition();
    outputs_.clear();
  }

  outstanding_rewrites_ = partitions_->partition_size();
  if (outstanding_rewrites_ == 0) {
    // The partitioning succeeded, but yielded zero rewrites.  Write out the
    // empty partition table and let any successor Rewrites run.
    rewrite_done_ = true;
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
    parent_->NestedRewriteDone();
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
    PropagateNestedAndHarvest();
  }
}

void RewriteContext::RewriteDone(
    RewriteSingleResourceFilter::RewriteResult result,
    int partition_index) {
  if (result == RewriteSingleResourceFilter::kTooBusy) {
    ok_to_write_output_partitions_ = false;
  } else {
    OutputPartition* partition =
        partitions_->mutable_partition(partition_index);
    bool optimizable = (result == RewriteSingleResourceFilter::kRewriteOk);
    partition->mutable_result()->set_optimizable(optimizable);
    if (!optimizable) {
      // TODO(sligocki): We are indescriminantly setting a 5min cache lifetime
      // for all failed rewrites. We should use the input resource's cache
      // lifetime instead. Or better yet, do conditional fetches of input
      // resources and only invalidate mapping if inputs change.
      int64 now_ms = Manager()->timer()->NowMs();
      partition->mutable_result()->set_origin_expiration_time_ms(
          now_ms + ResponseHeaders::kImplicitCacheTtlMs);
    }
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

void RewriteContext::PropagateNestedAndHarvest() {
  for (int i = 0, n = nested_.size(); i < n; ++i) {
    nested_[i]->Propagate(true);
  }
  Harvest();
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
      OutputPartition* partition = output_partition(p);
      for (int i = 0, n = partition->input_size(); i < n; ++i) {
        int slot_index = partition->input(i);
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
  Manager()->AddRewriteTask(new SuccessorsTask(this));
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
  OutputPartition* partition = output_partition(rewrite_index);
  for (int i = 0; i < partition->input_size(); ++i) {
    int slot_index = partition->input(i);
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
    driver_->DeleteRewriteContext(this);
  }
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
    fetch_.reset(new FetchContext(response_writer, response_headers, callback,
                                  output_resource, message_handler));
    Manager()->AddRewriteTask(new FetchTask(this));
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
