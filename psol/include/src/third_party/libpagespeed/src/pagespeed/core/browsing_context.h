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

#ifndef PAGESPEED_CORE_BROWSING_CONTEXT_H_
#define PAGESPEED_CORE_BROWSING_CONTEXT_H_

#include <map>
#include <vector>

#include "base/scoped_ptr.h"
#include "pagespeed/core/pagespeed_input.h"

namespace pagespeed {

class ActionUriGenerator;
class BrowsingContext;
class DomDocument;
class PagespeedInput;
class Resource;
class ResourceEvaluation;
class ResourceFetch;
class TopLevelBrowsingContext;

typedef std::vector<const BrowsingContext*> BrowsingContextVector;
typedef std::vector<const ResourceFetch*> ResourceFetchVector;
typedef std::vector<const ResourceEvaluation*> ResourceEvaluationVector;

// From the HTML5 spec:
// A browsing context describes the environment in which Document objects are
// presented to the user.
// A tab or window in a Web browser typically contains a browsing context, as
// does an iframe or frames in a frameset.
class BrowsingContext {
 public:
  virtual ~BrowsingContext();

  // Creates a nested browsing context and returns a modifiable pointer to it.
  // The resource is associated with this browsing contexts document, usually
  // the HTML resource of an iframe. Can be NULL if the nested browsing context
  // was created using scripting only.
  // The ownership remains at this BrowsingContext.
  BrowsingContext* AddNestedBrowsingContext(const Resource* resource);

  // Creates a resource fetch descriptor and returns a modifiable pointer to it.
  // Resources must have be added to the PagespeedInput before.
  // The ownership remains at this BrowsingContext.
  ResourceFetch* AddResourceFetch(const Resource* resource);

  // Creates a resource evaluation descriptor and returns a modifiable pointer
  // to it. Resources must have be added to the PagespeedInput before.
  // The ownership remains at this BrowsingContext.
  // For HTML resources, the first evaluation must be of the type PARSE_HTML.
  ResourceEvaluation* AddResourceEvaluation(const Resource* resource);

  // Registers a resource that is referenced in this browsing
  // context. The resource must belong to the same PagespeedInput this
  // BrowsingContext belongs to. Calling this method multiple times with the
  // same resource has no effect. If the specified resource redirect to another,
  // all resources (in-)directly redirected to will be added.
  bool RegisterResource(const Resource* child_resource);

  // Sets the time information of when the DOMContent event for this browsing
  // context was triggered. Note that only the ticks are used by pagespeed,
  // whereas the milliseconds are used for visualization purposes only.
  void SetEventDomContentTiming(int64 tick, int64 time_msec);

  // Sets the time information of when the onLoad event for this browsing
  // context was triggered. Note that only the ticks are used by pagespeed,
  // whereas the milliseconds are used for visualization purposes only.
  void SetEventLoadTiming(int64 tick, int64 time_msec);

  // Sets the DOM document for this browsing context. Ownership of the
  // DomDocument is transfered over to the BrowsingContext object.
  void AcquireDomDocument(DomDocument* document);

  // Finalizes this and all nested BrowsingContexts and makes them immutable.
  // Non-const methods cannot be called after calling Finalize.
  // Not yet finalized ResourceEvaluations and ResourceFetches are finalized as
  // well, but ignored otherwise.
  bool Finalize();

  // Returns true if this BrowsingContext is finalized.
  bool IsFinalized() const {
    return finalized_;
  }

  // Gets the resource associated with this browsing context's document. Usually
  // the HTML resource of an iframe, can be NULL if the iframe was created using
  // scripting only.
  const Resource* GetDocumentResourceOrNull() const;

  // Returns an URI uniquely identifying this BrowsingContext within the
  // pagespeed input. This URI is not equal to the HTTP(S) URL of the document
  // resource, but is a legal URI that uses a PageSpeed specific protocol.
  const std::string& GetBrowsingContextUri() const;

  // Returns the DOM document associated with this browsing context, or NULL if
  // it has not been set.
  const DomDocument* GetDomDocument() const;

  // Returns the parent browsing context for nested browsing contexts, or NULL
  // if this is the top-level context.
  const BrowsingContext* GetParentContext() const;

  // Returns all direct nested browsing contexts.
  bool GetNestedContexts(BrowsingContextVector* contexts) const;

  // Returns the number of directly nested browsing contexts.
  int32 GetNestedContextCount() const;

  // Returns the n-th nested BrowsingContext. Index must be in the range of
  // 0 <= index < GetNestedContextCount().
  const BrowsingContext& GetNestedContext(int index) const;

  // Returns a mutable copy of the n-th nested BrowsingContext. Index must be in
  // the range of 0 <= index < GetNestedContextCount().
  BrowsingContext* GetMutableNestedContext(int index);

  // Returns all the resources that are registered for this context, ie
  // resources that are referenced from the DOM and fetched in this browsing
  // context.
  bool GetResources(ResourceVector* resources) const;

  // Assigns all ResourceFetches registered for the specified Resource at this
  // BrowsingContext to the fetches vector. Returns true if one or more
  // ResourceFetch is being returned.
  // For most resources, there may be only one ResourceFetch on file per
  // BrowsingContext. However, in the case of non-cachable resources, there will
  // be multiple ResourceFetches.
  bool GetResourceFetches(const Resource& resource,
                          ResourceFetchVector* fetches) const;

  // Returns the number of ResourceFetches registered for the specified Resource
  // at this BrowsingContext.
  int32 GetResourceFetchCount(const Resource& resource) const;

  // Returns the n-th ResourceFetch registered for the specified Resource. index
  // must be in the range of 0 <= index < GetResourceFetchCount(resource).
  const ResourceFetch& GetResourceFetch(const Resource& resource,
                                        int index) const;

  // Returns the n-th mutable ResourceFetch registered for the specified
  // Resource. index must be in the range of
  // 0 <= index < GetResourceFetchCount(resource).
  ResourceFetch* GetMutableResourceFetch(const Resource& resource, int index);

  // Assigns all ResourceEvaluations registered for the specified Resource at
  // this BrowsingContext to the evaluations vector. Returns true if one or more
  // ResourceFetch is being returned.
  bool GetResourceEvaluations(const Resource& resource,
                              ResourceEvaluationVector* evaluations) const;

  // Returns the number of ResourceEvaluations registered for the specified
  // Resource at this BrowsingContext.
  int32 GetResourceEvaluationCount(const Resource& resource) const;

  // Returns the n-th ResourceEvaluation registered for the specified Resource.
  // index must be in the range of
  // 0 <= index < GetResourceEvaluationCount(resource).
  const ResourceEvaluation& GetResourceEvaluation(
      const Resource& resource, int index) const;

  // Returns the n-th mutable ResourceFetch registered for the specified
  // Resource. index must be in the range of
  // 0 <= index < GetResourceEvaluationCount(resource).
  ResourceEvaluation* GetMutableResourceEvaluation(
      const Resource& resource, int index);


  // Returns the tick when the document finished parsing.
  int64 GetDomContentTick() const {
    return event_dom_content_tick_;
  }

  // Returns the tick when the onLoad event fired.
  int64 GetLoadTick() const {
    return event_load_tick_;
  }

  // Serializes this BrowsingContext, all ResourceFetch and ResourceEvaluation
  // and nested BrowsingContext to the specified BrowsingContextData.
  bool SerializeData(BrowsingContextData* data) const;

 protected:
  // Clients must create instances either via TopLevelBrowsingContext or
  // CreateNestedBrowsingContext().
  BrowsingContext(const Resource* document_resource,
                  const BrowsingContext* parent_context,
                  TopLevelBrowsingContext* top_level_context,
                  ActionUriGenerator* action_uri_generator,
                  const PagespeedInput* pagespeed_input);

  // Registers a (nested) browsing context with the top-level context. This
  // ensures that the TopLevelBrowsingContext knows about the
  // context->GetUri() -> context mapping.
  virtual bool RegisterBrowsingContext(const BrowsingContext* context);

  // Registers a ResourceFetch with the top-level context. This ensures that the
  // TopLevelBrowsingContext knows about the context->GetUri() -> context
  // mapping.
  virtual bool RegisterResourceFetch(const ResourceFetch* fetch);

  // Registers a ResourceEvaluation with the top-level context. This ensures
  // that the TopLevelBrowsingContext knows about the
  // context->GetUri() -> context mapping.
  virtual bool RegisterResourceEvaluation(const ResourceEvaluation* eval);

  ActionUriGenerator* action_uri_generator() const {
    return action_uri_generator_;
  }

 private:
  typedef std::map<const Resource*, std::vector<ResourceFetch*> >
      ResourceFetchMap;
  typedef std::map<const Resource*, std::vector<ResourceEvaluation*> >
      ResourceEvalMap;

  const PagespeedInput* const pagespeed_input_;

  // This is owned by the TopLevelBrowsingContext.
  ActionUriGenerator* const action_uri_generator_;

  bool finalized_;

  TopLevelBrowsingContext* const top_level_context_;

  std::string uri_;

  ResourceSet resources_;
  std::vector<BrowsingContext*> nested_contexts_;
  const BrowsingContext* parent_context_;
  const Resource* document_resource_;
  scoped_ptr<DomDocument> document_;

  int64 event_dom_content_msec_;
  int64 event_dom_content_tick_;
  int64 event_load_msec_;
  int64 event_load_tick_;

  ResourceFetchMap resource_fetch_map_;
  ResourceEvalMap resource_evaluation_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingContext);
};

// The top-level browsing context is the browsing context of the primary
// resource.
class TopLevelBrowsingContext : public BrowsingContext {
 public:
  // Creates a top-level browsing context.
  TopLevelBrowsingContext(const Resource* document_resource,
                          const PagespeedInput* pagespeed_input);
  virtual ~TopLevelBrowsingContext();

  // Returns the (nested) BrowsingContext identified by the specified URI.
  const BrowsingContext* FindBrowsingContext(
      const std::string& context_uri) const;

  // Returns the ResourceFetch identified by the specified URI. The
  // ResourceFetch might belong to a nested BrowsingContext.
  const ResourceFetch* FindResourceFetch(const std::string& fetch_uri) const;

  // Returns the ResourceEvaluation identified by the specified URI. The
  // ResourceEvaluation might belong to a nested BrowsingContext.
  const ResourceEvaluation* FindResourceEvaluation(
      const std::string& eval_uri) const;

 protected:
  // These methods are protected virtual in order to expose them only to
  // BrowsingContext instead of making it public API.
  virtual bool RegisterBrowsingContext(const BrowsingContext* context);
  virtual bool RegisterResourceFetch(const ResourceFetch* fetch);
  virtual bool RegisterResourceEvaluation(const ResourceEvaluation* eval);

 private:
  std::map<std::string, const BrowsingContext*> uri_browsing_context_map_;
  std::map<std::string, const ResourceFetch*> uri_resource_fetch_map_;
  std::map<std::string, const ResourceEvaluation*> uri_resource_eval_map_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelBrowsingContext);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_BROWSING_CONTEXT_H_
