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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_CALLBACK_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_CALLBACK_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "net/instaweb/rewriter/public/central_controller_interface.h"

namespace net_instaweb {

// CentralControllerCallback is a Function specialization that encapsulates
// a call to the CentralController. Users are expected to interact with this via
// a purpose-specific subclass. See CentralControllerInterfaceAdapter for
// examples.
//
// Calls to the CentralController are expected to go via an RPC interface.
// Since the Run operation may be expensive, it is important to not block
// the RPC dispatcher thread, so this callback "re-queues" itself onto
// a QueuedWorkerPool::Sequence to do the actual work. This is very similar
// to Sequence::AddFunction.
//
// If the CentralController successfully processes the request, Run() will be
// called. At this point, the CentralController may have allocated resources
// which must be returned. However, it is possible that the callback will be
// load-shed from the Sequence. It is important that the CentralController is
// *always* notified when it can reclaim the resources, even if the actual
// operation is load-shed. Thus, when the CentralController calls back with
// success (the first time Run() is invoked) a "TransactionContext" is created.
// The TransactionContext will be deleted once the operation is complete. It
// must guarantee that it will have notified the CentralController to reclaim
// any resources by the time it is deleted.
//
// The CentralController also has the option of denying the operation, which
// will result in a call to Cancel(). This will also happen in the case of an
// RPC error. In either case, no TransactionContext will be created, since there
// is no transaction to proceed and therefore no resources to return.
//
// The TransactionContext is also the way a caller can signal information
// to the CentralController. For instance, it may implement a Success() or
// Failure() method. For the case where the operation performed by the caller
// outlives the Run() callback, a scoped_ptr to the context is passed into
// RunImpl(), which may "steal" the pointer.

template <typename TransactionContext>
class CentralControllerCallback : public Function {
 public:
  virtual ~CentralControllerCallback();

 protected:
  explicit CentralControllerCallback(QueuedWorkerPool::Sequence* sequence);

  // Function interface. These will be invoked on the RPC thread, so must be
  // quick. They just enqueue calls on sequence_ to the actual implementations
  // (RunAfterRequeue & CancelAfterRequeue).
  virtual void Run();
  virtual void Cancel();

 private:
  // Subclasses should implement whatever functionality they need in these.
  // They are equivalent to the Run() and Cancel() methods on Function.
  virtual void RunImpl(scoped_ptr<TransactionContext>* context) = 0;
  virtual void CancelImpl() = 0;

  // Factory for the TransactionContext. Invoked on the RPC thread when the
  // CentralController invokes Run(). Must not do anything expensive.
  virtual TransactionContext* CreateTransactionContext(
      CentralControllerInterface* interface) = 0;

  // This will be called by the CentralControllerInterfaceAdapter before
  // the Function is dispatched.
  void SetCentralControllerInterface(CentralControllerInterface* interface) {
    CHECK(controller_interface_ == NULL || controller_interface_ == interface);
    controller_interface_ = interface;
  }

  // Invoked via sequence_ to do the typical Function operations.
  void RunAfterRequeue();
  void CancelAfterRequeue();

  QueuedWorkerPool::Sequence* sequence_;
  CentralControllerInterface* controller_interface_;
  scoped_ptr<TransactionContext> context_;

  friend class CentralControllerInterfaceAdapter;

  DISALLOW_COPY_AND_ASSIGN(CentralControllerCallback);
};

template <typename TransactionContext>
CentralControllerCallback<TransactionContext>::CentralControllerCallback(
    QueuedWorkerPool::Sequence* sequence)
    : sequence_(sequence), controller_interface_(NULL) {
  set_delete_after_callback(false);
}

template <typename TransactionContext>
CentralControllerCallback<TransactionContext>::~CentralControllerCallback() { }

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::Run() {
  // We were just called back by the server, so create a TransationContext.
  CHECK(context_.get() == NULL);
  context_.reset(CreateTransactionContext(controller_interface_));
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
  CHECK(context_.get() != NULL);
  RunImpl(&context_);
  delete this;
}

template <typename TransactionContext>
void CentralControllerCallback<TransactionContext>::CancelAfterRequeue() {
  CancelImpl();
  delete this;
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_CALLBACK_H_
