// Copyright 2010 Google Inc.
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

#ifndef MOD_SPDY_APACHE_POOL_UTIL_H_
#define MOD_SPDY_APACHE_POOL_UTIL_H_

#include <string>

#include "apr_pools.h"
#include "base/logging.h"

namespace mod_spdy {

/**
 * Wrapper object that creates a new apr_pool_t and then destroys it when
 * deleted (handy for creating a local apr_pool_t on the stack).
 *
 * Example usage:
 *
 *   apr_status_t SomeFunction() {
 *     LocalPool local;
 *     char* buffer = apr_palloc(local.pool(), 1024);
 *     // Do stuff with buffer; it will dealloc when we leave this scope.
 *     return APR_SUCCESS;
 *   }
 */
class LocalPool {
 public:
  LocalPool() : pool_(NULL) {
    // apr_pool_create() only fails if we run out of memory.  However, we make
    // no effort elsewhere in this codebase to deal with running out of memory,
    // so there's no sense in dealing with it here.  Instead, just assert that
    // pool creation succeeds.
    const apr_status_t status = apr_pool_create(&pool_, NULL);
    CHECK(status == APR_SUCCESS);
    CHECK(pool_ != NULL);
  }

  ~LocalPool() {
    apr_pool_destroy(pool_);
  }

  apr_pool_t* pool() const { return pool_; }

 private:
  apr_pool_t* pool_;

  DISALLOW_COPY_AND_ASSIGN(LocalPool);
};

// Helper function for PoolRegisterDelete.
template <class T>
apr_status_t DeletionFunction(void* object) {
  delete static_cast<T*>(object);
  return APR_SUCCESS;
}

// Register a C++ object to be deleted with a pool.
template <class T>
void PoolRegisterDelete(apr_pool_t* pool, T* object) {
  // Note that the "child cleanup" argument below doesn't apply to us, so we
  // use apr_pool_cleanup_null, which is a no-op cleanup function.
  apr_pool_cleanup_register(pool, object,
                            DeletionFunction<T>,  // cleanup function
                            apr_pool_cleanup_null);  // child cleanup
}

// Un-register a C++ object from deletion with a pool.  Essentially, this
// undoes a previous call to PoolRegisterDelete with the same pool and object.
template <class T>
void PoolUnregisterDelete(apr_pool_t* pool, T* object) {
  apr_pool_cleanup_kill(pool, object, DeletionFunction<T>);
}

// Return a string describing the given APR status code.
std::string AprStatusString(apr_status_t status);

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_POOL_UTIL_H_
