// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RESOURCE_EVALUATION_H_
#define PAGESPEED_CORE_RESOURCE_EVALUATION_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include "pagespeed/proto/resource.pb.h"

namespace pagespeed {

class BrowsingContext;
class PagespeedInput;
class Resource;
class ResourceFetch;
class ResourceEvaluationData;
class ResourceEvaluationConstraint;
class ResourceEvaluationConstraintData;

typedef std::vector<const ResourceEvaluationConstraint*>
    EvaluationConstraintVector;

// Describes the (partial) evaluation of a resource.
class ResourceEvaluation {
 public:
  // Do not create instances directly, but rather obtain them using
  // BrowsingContext::CreateResourceEvaluation().
  ResourceEvaluation(const std::string& uri,
                     const BrowsingContext* context,
                     const Resource* resource,
                     const PagespeedInput* pagespeed_input);
  virtual ~ResourceEvaluation();

  // Adds a constraint to this evaluation.
  ResourceEvaluationConstraint* AddConstraint();

  // Sets the timing information for this resource evaluation. Pass in -1 for
  // msecs / ticks that are unknown.
  void SetTiming(int64 start_tick, int64 start_time_msec, int64 finish_tick,
                 int64 finish_time_msec);

  // Sets the ResourceFetch that loaded the resource being evaluated here.
  bool SetFetch(const ResourceFetch& fetch);

  // Sets if the media type matched.
  void SetIsMatchingMediaType(bool is_matching_media_type);

  // Sets if this is a script is asynchronously executed (the async
  // attribute of the script tag). Do not set if the resource is not a script.
  void SetIsAsync(bool is_async);

  // Indicates if this is a script's execution was deferred (the defer attribute
  // of the script tag). o not set if the resource is not a script.
  void SetIsDefer(bool is_defer);

  // Sets the start/end line within the resource which is being evaluated.
  // Set both start and end to -1 if the complete resource is being evaluated at
  // once.
  void SetEvaluationLines(int32 start_line, int32 end_line);

  void SetEvaluationType(EvaluationType type);

  // Finalizes this ResourceEvaluation and makes it immutable. Non-const methods
  // cannot be called after calling Finalize.
  bool Finalize();

  // Returns true if this ResourceEvaluation is finalized.
  bool IsFinalized() const {
    return finalized_;
  }

  // Returns the URI uniquely identifying this evaluation.
  const std::string& GetResourceEvaluationUri() const {
    return data_->uri();
  }

  // Returns the Resource to which this evaluation applies.
  const Resource& GetResource() const {
    return *resource_;
  }

  // Convenience access to the resource type. This is equivalent to
  // GetResource().GetResourceType().
  ResourceType GetResourceType() const;

  // Returns the type of the evaluation, which is different from the resource
  // type.
  EvaluationType GetEvaluationType() const {
    return data_->type();
  }

  // Returns the ResourceFetch that loaded the resource being evaluated here.
  const ResourceFetch* GetFetch() const;

  // Assigns all evaluation constraints to the constraints vector. Returns true
  // if one or more ResourceEvaluationConstraint is being returned. The
  // ownership of the constraints remains with this ResourceEvaluation.
  bool GetConstraints(EvaluationConstraintVector* constraints) const;

  // Returns the number of ResourceEvaluationConstraint registered for this
  // evaluation.
  int32 GetConstraintCount() const;

  // Returns the n-th ResourceEvaluationConstraint registered for this
  // evaluation. index must be in the range 0 <= index < GetConstraintCount().
  // The ownership of the constraints remains with this ResourceEvaluation.
  const ResourceEvaluationConstraint& GetConstraint(int index) const;

  // Returns the n-th mutable ResourceEvaluationConstraint registered for this
  // evaluation. index must be in the range 0 <= index < GetConstraintCount().
  // The ownership of the constraints remains with this ResourceEvaluation.
  ResourceEvaluationConstraint* GetMutableConstraint(int index);

  // Returns the start line within this resource which is being evaluated.
  // -1 if the complete resource is being evaluated at once.
  int32 GetEvaluationStartLine() const {
    return data_->block_start_line();
  }

  // Returns the end line within the resource which is being evaluated.
  // -1 if unknown or if the complete resource is being evaluated at once.
  int32 GetEvaluationEndLine() const {
    return data_->block_end_line();
  }

  // Gets the tick value that describes the order of this evaluation start
  // event, relative to other load and/or eval events. The number does not
  // represent the absolute start time.
  int64 GetStartTick() const {
    return data_->start().tick();
  }

  // Gets the tick value that describes the order of this evaluation finish
  // event, relative to other load and/or eval events. The number does not
  // represent the absolute finish time.
  int64 GetFinishTick() const {
    return data_->finish().tick();
  }

  // Indicates if this is a script's async attribute of the script tag was set.
  // False if the resource is not a script.
  bool IsAsync() const {
    return data_->is_async();
  }

  // Indicates if this is a script's defer attribute of the script tag was set.
  // False if the resource is not a script.
  bool IsDefer() const {
    return data_->is_defer();
  }

  // True if the CSS media type matched. False if the resource is not a CSS.
  bool IsMatchingMediaType() const {
    return data_->is_matching_media_type();
  }

  // Serializes this ResourceEvaluation and all the constraints to the specified
  // ResourceEvaluationData message.
  bool SerializeData(ResourceEvaluationData* data) const;

 private:
  const PagespeedInput* pagespeed_input_;
  const Resource* resource_;
  const BrowsingContext* context_;

  bool finalized_;

  std::vector<ResourceEvaluationConstraint*> constraints_;

  scoped_ptr<ResourceEvaluationData> data_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEvaluation);
};

// Describes a precondition that has to be met before a ResourceEvaluation can
// take place.
class ResourceEvaluationConstraint {
 public:
  // Do not create instances directly, but rather obtain them using
  // ResourceEvaluation::AddConstraint()
  explicit ResourceEvaluationConstraint(const PagespeedInput* pagespeed_input);
  virtual ~ResourceEvaluationConstraint();

  // Sets a ResourceEvaluation, which must be completed according to
  // GetConstraintType before this evaluation can be started.
  bool SetPredecessor(const ResourceEvaluation* predecessor);

  // The type of precondition to be met before the evaluation can take place.
  void SetConstraintType(EvaluationConstraintType constraint_type) {
    data_->set_type(constraint_type);
  }

  // Returns the type of precondition to be met before the evaluation can take
  // place. Also see GetPredecessor()
  EvaluationConstraintType GetConstraintType() const {
    return data_->type();
  }

  // Returns the ResourceEvaluationData, which must be completed according to
  // GetConstraintType() before this evaluation can be started.
  const ResourceEvaluation* GetPredecessor() const;

  // Serializes this ResourceEvaluationConstraintto the specified
  // ResourceEvaluationConstraintData message.
  bool SerializeData(ResourceEvaluationConstraintData* data) const;

 private:
  const PagespeedInput* pagespeed_input_;
  scoped_ptr<ResourceEvaluationConstraintData> data_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEvaluationConstraint);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_EVALUATION_H_
