/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APR_RESLIST_H
#define APR_RESLIST_H

/** 
 * @file apr_reslist.h
 * @brief APR-UTIL Resource List Routines
 */

#include "apr.h"
#include "apu.h"
#include "apr_pools.h"
#include "apr_errno.h"
#include "apr_time.h"

#if APR_HAS_THREADS

/**
 * @defgroup APR_Util_RL Resource List Routines
 * @ingroup APR_Util
 * @{
 * @warning
 * <strong><em>Resource list data types and routines are only available when
 * threads are enabled (i.e. APR_HAS_THREADS is not zero).</em></strong>
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Opaque resource list object */
typedef struct apr_reslist_t apr_reslist_t;

/* Generic constructor called by resource list when it needs to create a
 * resource.
 * @param resource opaque resource
 * @param param flags
 * @param pool  Pool
 */
typedef apr_status_t (*apr_reslist_constructor)(void **resource, void *params,
                                                apr_pool_t *pool);

/* Generic destructor called by resource list when it needs to destroy a
 * resource.
 * @param resource opaque resource
 * @param param flags
 * @param pool  Pool
 */
typedef apr_status_t (*apr_reslist_destructor)(void *resource, void *params,
                                               apr_pool_t *pool);

/**
 * Create a new resource list with the following parameters:
 * @param reslist An address where the pointer to the new resource
 *                list will be stored.
 * @param min Allowed minimum number of available resources. Zero
 *            creates new resources only when needed.
 * @param smax Resources will be destroyed to meet this maximum
 *             restriction as they expire.
 * @param hmax Absolute maximum limit on the number of total resources.
 * @param ttl If non-zero, sets the maximum amount of time in microseconds a
 *            resource may be available while exceeding the soft limit.
 * @param con Constructor routine that is called to create a new resource.
 * @param de Destructor routine that is called to destroy an expired resource.
 * @param params Passed to constructor and deconstructor
 * @param pool The pool from which to create this resource list. Also the
 *             same pool that is passed to the constructor and destructor
 *             routines.
 * @warning If you're creating a sub-pool of the pool passed into this
 *          function in your constructor, you will need to follow some rules
 *          when it comes to destruction of that sub-pool, as calling
 *          apr_pool_destroy() outright on it in your destructor may create
 *          double free situations. That is because by the time destructor is
 *          called, the sub-pool may have already been destroyed. This also
 *          means that in the destructor, memory from the sub-pool should be
 *          treated as invalid. For examples of how to do this correctly, see
 *          mod_dbd of Apache 2.2 and memcache support in APR Util 1.3.
 */
APU_DECLARE(apr_status_t) apr_reslist_create(apr_reslist_t **reslist,
                                             int min, int smax, int hmax,
                                             apr_interval_time_t ttl,
                                             apr_reslist_constructor con,
                                             apr_reslist_destructor de,
                                             void *params,
                                             apr_pool_t *pool);

/**
 * Destroy the given resource list and all resources controlled by
 * this list.
 * FIXME: Should this block until all resources become available,
 *        or maybe just destroy all the free ones, or maybe destroy
 *        them even though they might be in use by something else?
 *        Currently it will abort if there are resources that haven't
 *        been released, so there is an assumption that all resources
 *        have been released to the list before calling this function.
 * @param reslist The reslist to destroy
 */
APU_DECLARE(apr_status_t) apr_reslist_destroy(apr_reslist_t *reslist);

/**
 * Retrieve a resource from the list, creating a new one if necessary.
 * If we have met our maximum number of resources, we will block
 * until one becomes available.
 */
APU_DECLARE(apr_status_t) apr_reslist_acquire(apr_reslist_t *reslist,
                                              void **resource);

/**
 * Return a resource back to the list of available resources.
 */
APU_DECLARE(apr_status_t) apr_reslist_release(apr_reslist_t *reslist,
                                              void *resource);

/**
 * Set the timeout the acquire will wait for a free resource
 * when the maximum number of resources is exceeded.
 * @param reslist The resource list.
 * @param timeout Timeout to wait. The zero waits forever.
 */
APU_DECLARE(void) apr_reslist_timeout_set(apr_reslist_t *reslist,
                                          apr_interval_time_t timeout);

/**
 * Return the number of outstanding resources.
 * @param reslist The resource list.
 */
APU_DECLARE(apr_uint32_t) apr_reslist_acquired_count(apr_reslist_t *reslist);

/**
 * Invalidate a resource in the pool - e.g. a database connection
 * that returns a "lost connection" error and can't be restored.
 * Use this instead of apr_reslist_release if the resource is bad.
 */
APU_DECLARE(apr_status_t) apr_reslist_invalidate(apr_reslist_t *reslist,
                                                 void *resource);


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* APR_HAS_THREADS */

#endif  /* ! APR_RESLIST_H */
