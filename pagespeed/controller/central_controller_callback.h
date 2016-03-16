// Copyright 2015 Google Inc.
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
//
// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_CALLBACK_H_
#define PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_CALLBACK_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/thread/sequence.h"

namespace net_instaweb {

// CentralControllerCallback is a Function specialization that encapsulates
// a call to the CentralController. Users are expected to interact with this via
// a purpose-specific subclass, eg: ExpensiveOperationCallback.
//
// Calls to the CentralController are expected to go via an RPC interface.
// Since the Run operation may be expensive, it is important to not block
// the RPC dispatcher thread, so this callback "re-queues" itself onto
// a Sequence to do the actual work.
//
// If the CentralController successfully processes the request, Run() will be
// called. At this point, the CentralController may have allocated resources
// which must be returned. However, it is also possible that the callback will
// be load-shed from the Sequence. It is important that the CentralController is
// *always* notified when it can reclaim the resources, even if the actual
// operation is load-shed. This is where the TransactionContext comes in; it
// guarantees to notify the controller to release any held resources exactly
// once, either upon destruction of the context or by explicit calls from the
// consumer class(es). Construction and exact semantics of the
// TransactionContext are managed by the CentralController implementation.
//
// The TransactionContext is also the way a caller can signal information
// to the CentralController. For instance, it may implement a Success() or
// Failure() method. For the case where the operation performed by the caller
// outlives the Run() callback, a scoped_ptr to the context is passed into
// RunImpl(), which may "steal" the pointer.
//
// The CentralController also has the option of denying the operation, which
// will result in a call to Cancel(). This will also happen in the case of an
// RPC error. It is the responsibility of the TransactionContext to clean up
// in the case where an RPC failure occurs partway through a transaction.

template <typename TransactionContext>
class CentralControllerCallback : public Function {
 public:
  virtual ~CentralControllerCallback();

  // Called by the CentralController at some point before Run or Cancel.
  // Takes ownership of the transaction context.
  // TODO(cheesy): It would be nice if this wasn't public, but that causes
  // mutual visibility headaches with the context implementations.
  void SetTransactionContext(TransactionContext* ctx);

 protected:
  explicit CentralControllerCallback(Sequence* sequence);

  // Function interface. These will be invoked on the RPC thread, so must be
  // quick. They just enqueue calls on sequence_ to the actual implementations
  // (RunAfterRequeue & CancelAfterRequeue).
  virtual void Run() /* override */;
  virtual void Cancel() /* override */;

 private:
  // Subclasses should implement whatever functionality they need in these.
  // They are equivalent to the Run() and Cancel() methods on Function.
  virtual void RunImpl(scoped_ptr<TransactionContext>* context) = 0;
  virtual void CancelImpl() = 0;

  // Invoked via sequence_ to do the typical Function operations.
  void RunAfterRequeue();
  void CancelAfterRequeue();

  Sequence* sequence_;
  scoped_ptr<TransactionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(CentralControllerCallback);
};

template <typename TransactionContext>
CentralControllerCallback<TransactionContext>::CentralControllerCallback(
    Sequence* sequence) : sequence_(sequence) {
  set_delete_after_callback(false);
}

template <typename TransactionContext>
CentralControllerCallback<TransactionContext>::~CentralControllerCallback() { }

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::Run() {
  CHECK(context_ != NULL);
  // Now enqueue the call to actually run.
  // Will synchronously call CancelAfterRequeue if sequence_ is shutdown.
  sequence_->Add(MakeFunction(this,
      &CentralControllerCallback<TransactionContext>::RunAfterRequeue,
      &CentralControllerCallback<TransactionContext>::CancelAfterRequeue));
}

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::Cancel() {
  // Server rejected the request or RPC error. Enqueue a Cancellation.
  // Will synchronously call CancelAfterRequeue if sequence_ is shutdown.
  sequence_->Add(MakeFunction(this,
      &CentralControllerCallback<TransactionContext>::CancelAfterRequeue,
      &CentralControllerCallback<TransactionContext>::CancelAfterRequeue));
}

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::RunAfterRequeue() {
  // Actually run the callback. Note that RunImpl may steal the pointer.
  CHECK(context_ != NULL);
  RunImpl(&context_);
  delete this;
}

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::CancelAfterRequeue() {
  CancelImpl();
  delete this;
}

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::SetTransactionContext(
    TransactionContext* context) {
  CHECK(context_ == NULL);
  context_.reset(context);
}

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_CALLBACK_H_
