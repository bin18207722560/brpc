// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// bthread - An M:N threading library to make applications more concurrent.

// Date: Tue Jul 10 17:40:58 CST 2012
// 授权给 Apache Software Foundation (ASF) 的一个或多个贡献者许可协议。
// 有关版权归属的更多信息，请参阅随本作品分发的 NOTICE 文件。
// ASF 根据 Apache License, Version 2.0（“许可证”）授权此文件；
// 除非符合许可证，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据许可证分发的软件
// 是按“原样”分发的，没有任何明示或暗示的担保或条件。
// 有关许可证下特定语言的权限和限制，请参阅许可证。
//
// bthread - 一个 M:N threading 库，用于使应用程序更具并发性。
//
// 日期: 2012年7月10日 星期二

#ifndef BTHREAD_BTHREAD_H
#define BTHREAD_BTHREAD_H

#include <pthread.h>
#include <sys/socket.h>
#include "bthread/types.h"
#include "bthread/errno.h"

#if defined(__cplusplus)
#include <iostream>
#include "bthread/mutex.h"        // use bthread_mutex_t in the RAII way
#endif // __cplusplus

#include "bthread/id.h"

#if defined(__cplusplus) && defined(BRPC_BTHREAD_TRACER)
namespace bthread {
// Assign a TaskMeta to the pthread and set the state to Running,
// so that `stack_trace()' can trace the call stack of the pthread.
bthread_t init_for_pthread_stack_trace();

// Trace the call stack of the bthread, or pthread which has been
// initialized by `init_for_pthread_stack_trace()'.
void stack_trace(std::ostream& os, bthread_t tid);
std::string stack_trace(bthread_t tid);
} // namespace bthread
#endif // __cplusplus && BRPC_BTHREAD_TRACER

__BEGIN_DECLS

// Create bthread `fn(args)' with attributes `attr' and put the identifier into
// `tid'. Switch to the new thread and schedule old thread to run. Use this
// function when the new thread is more urgent.
// Returns 0 on success, errno otherwise.
extern int bthread_start_urgent(bthread_t* __restrict tid,
                                const bthread_attr_t* __restrict attr,
                                void * (*fn)(void*),
                                void* __restrict args);

// Create bthread `fn(args)' with attributes `attr' and put the identifier into
// `tid'. This function behaves closer to pthread_create: after scheduling the
// new thread to run, it returns. In another word, the new thread may take
// longer time than bthread_start_urgent() to run.
// Return 0 on success, errno otherwise.
extern int bthread_start_background(bthread_t* __restrict tid,
                                    const bthread_attr_t* __restrict attr,
                                    void * (*fn)(void*),
                                    void* __restrict args);

// Wake up operations blocking the thread. Different functions may behave
// differently:
//   bthread_usleep(): returns -1 and sets errno to ESTOP if bthread_stop()
//                     is called, or to EINTR otherwise.
//   butex_wait(): returns -1 and sets errno to EINTR
//   bthread_mutex_*lock: unaffected (still blocking)
//   bthread_cond_*wait: wakes up and returns 0.
//   bthread_*join: unaffected.
// Common usage of interruption is to make a thread to quit ASAP.
//    [Thread1]                  [Thread2]
//   set stopping flag
//   bthread_interrupt(Thread2)
//                               wake up
//                               see the flag and quit
//                               may block again if the flag is unchanged
// bthread_interrupt() guarantees that Thread2 is woken up reliably no matter
// how the 2 threads are interleaved.
// Returns 0 on success, errno otherwise.
extern int bthread_interrupt(bthread_t tid, bthread_tag_t tag = BTHREAD_TAG_DEFAULT);

// Make bthread_stopped() on the bthread return true and interrupt the bthread.
// Note that current bthread_stop() solely sets the built-in "stop flag" and
// calls bthread_interrupt(), which is different from earlier versions of
// bthread, and replaceable by user-defined stop flags plus calls to
// bthread_interrupt().
// Returns 0 on success, errno otherwise.
extern int bthread_stop(bthread_t tid);

// Returns 1 iff bthread_stop(tid) was called or the thread does not exist,
// 0 otherwise.
extern int bthread_stopped(bthread_t tid);

// Returns identifier of caller if caller is a bthread, 0 otherwise(Id of a
// bthread is never zero)
extern bthread_t bthread_self(void);

// Compare two bthread identifiers.
// Returns a non-zero value if t1 and t2 are equal, zero otherwise.
extern int bthread_equal(bthread_t t1, bthread_t t2);

// Terminate calling bthread/pthread and make `retval' available to any
// successful join with the terminating thread. This function does not return.
extern void bthread_exit(void* retval) __attribute__((__noreturn__));

// Make calling thread wait for termination of bthread `bt'. Return immediately
// if `bt' is already terminated.
// Notes:
//  - All bthreads are "detached" but still joinable.
//  - *bthread_return is always set to null. If you need to return value
//    from a bthread, pass the value via the `args' created the bthread.
//  - bthread_join() is not affected by bthread_interrupt.
// Returns 0 on success, errno otherwise.
extern int bthread_join(bthread_t bt, void** bthread_return);

// Track and join many bthreads.
// Notice that all bthread_list* functions are NOT thread-safe.
extern int  bthread_list_init(bthread_list_t* list,
                              unsigned size, unsigned conflict_size);
extern void bthread_list_destroy(bthread_list_t* list);
extern int  bthread_list_add(bthread_list_t* list, bthread_t tid);
extern int bthread_list_stop(bthread_list_t* list);
extern int  bthread_list_join(bthread_list_t* list);

// ------------------------------------------
// Functions for handling attributes.
// ------------------------------------------

// Initialize thread attribute `attr' with default attributes.
extern int bthread_attr_init(bthread_attr_t* attr);

// Destroy thread attribute `attr'.
extern int bthread_attr_destroy(bthread_attr_t* attr);

// Initialize bthread attribute `attr' with attributes corresponding to the
// already running bthread `bt'.  It shall be called on unitialized `attr'
// and destroyed with bthread_attr_destroy when no longer needed.
extern int bthread_getattr(bthread_t bt, bthread_attr_t* attr);

// ---------------------------------------------
// Functions for scheduling control.
// ---------------------------------------------

// Get number of worker pthreads
extern int bthread_getconcurrency(void);

// Set number of worker pthreads to `num'. After a successful call,
// bthread_getconcurrency() shall return new set number, but workers may
// take some time to quit or create.
// NOTE: currently concurrency cannot be reduced after any bthread created.
extern int bthread_setconcurrency(int num);

// Get number of worker pthreads by tag
extern int bthread_getconcurrency_by_tag(bthread_tag_t tag);

// Set number of worker pthreads to `num' for specified tag
extern int bthread_setconcurrency_by_tag(int num, bthread_tag_t tag);

// Yield processor to another bthread. 
// Notice that current implementation is not fair, which means that 
// even if bthread_yield() is called, suspended threads may still starve.
extern int bthread_yield(void);

// Suspend current thread for at least `microseconds'
// Interruptible by bthread_interrupt().
extern int bthread_usleep(uint64_t microseconds);

// ---------------------------------------------
// Functions for mutex handling.
// ---------------------------------------------

// Initialize `mutex' using attributes in `mutex_attr', or use the
// default values if later is NULL.
// NOTE: mutexattr is not used in current mutex implementation. User shall
//       always pass a NULL attribute.
extern int bthread_mutex_init(bthread_mutex_t* __restrict mutex,
                              const bthread_mutexattr_t* __restrict attr);

// Destroy `mutex'.
extern int bthread_mutex_destroy(bthread_mutex_t* mutex);

// Try to lock `mutex'.
extern int bthread_mutex_trylock(bthread_mutex_t* mutex);

// Wait until lock for `mutex' becomes available and lock it.
extern int bthread_mutex_lock(bthread_mutex_t* mutex);

// Wait until lock becomes available and lock it or time exceeds `abstime'
extern int bthread_mutex_timedlock(bthread_mutex_t* __restrict mutex,
                                   const struct timespec* __restrict abstime);

// Unlock `mutex'.
extern int bthread_mutex_unlock(bthread_mutex_t* mutex);

extern int bthread_mutexattr_init(bthread_mutexattr_t* attr);

// Disable the contention profile of the mutex.
extern int bthread_mutexattr_disable_csite(bthread_mutexattr_t* attr);

extern int bthread_mutexattr_destroy(bthread_mutexattr_t* attr);

// -----------------------------------------------
// Functions for handling conditional variables.
// -----------------------------------------------

// Initialize condition variable `cond' using attributes `cond_attr', or use
// the default values if later is NULL.
// NOTE: cond_attr is not used in current condition implementation. User shall
//       always pass a NULL attribute.
extern int bthread_cond_init(bthread_cond_t* __restrict cond,
                             const bthread_condattr_t* __restrict cond_attr);

// Destroy condition variable `cond'.
extern int bthread_cond_destroy(bthread_cond_t* cond);

// Wake up one thread waiting for condition variable `cond'.
extern int bthread_cond_signal(bthread_cond_t* cond);

// Wake up all threads waiting for condition variables `cond'.
extern int bthread_cond_broadcast(bthread_cond_t* cond);

// Wait for condition variable `cond' to be signaled or broadcast.
// `mutex' is assumed to be locked before.
extern int bthread_cond_wait(bthread_cond_t* __restrict cond,
                             bthread_mutex_t* __restrict mutex);

// Wait for condition variable `cond' to be signaled or broadcast until
// `abstime'. `mutex' is assumed to be locked before.  `abstime' is an
// absolute time specification; zero is the beginning of the epoch
// (00:00:00 GMT, January 1, 1970).
extern int bthread_cond_timedwait(
    bthread_cond_t* __restrict cond,
    bthread_mutex_t* __restrict mutex,
    const struct timespec* __restrict abstime);

// -------------------------------------------
// Functions for handling read-write locks.
// -------------------------------------------

// Initialize read-write lock `rwlock' using attributes `attr', or use
// the default values if later is NULL.
extern int bthread_rwlock_init(bthread_rwlock_t* __restrict rwlock,
                               const bthread_rwlockattr_t* __restrict attr);

// Destroy read-write lock `rwlock'.
extern int bthread_rwlock_destroy(bthread_rwlock_t* rwlock);

// Acquire read lock for `rwlock'.
extern int bthread_rwlock_rdlock(bthread_rwlock_t* rwlock);

// Try to acquire read lock for `rwlock'.
extern int bthread_rwlock_tryrdlock(bthread_rwlock_t* rwlock);

// Try to acquire read lock for `rwlock' or return after specfied time.
extern int bthread_rwlock_timedrdlock(bthread_rwlock_t* __restrict rwlock,
                                      const struct timespec* __restrict abstime);

// Acquire write lock for `rwlock'.
extern int bthread_rwlock_wrlock(bthread_rwlock_t* rwlock);

// Try to acquire write lock for `rwlock'.
extern int bthread_rwlock_trywrlock(bthread_rwlock_t* rwlock);

// Try to acquire write lock for `rwlock' or return after specfied time.
extern int bthread_rwlock_timedwrlock(bthread_rwlock_t* __restrict rwlock,
                                      const struct timespec* __restrict abstime);

// Unlock `rwlock'.
extern int bthread_rwlock_unlock(bthread_rwlock_t* rwlock);

// ---------------------------------------------------
// Functions for handling read-write lock attributes.
// ---------------------------------------------------

// Initialize attribute object `attr' with default values.
extern int bthread_rwlockattr_init(bthread_rwlockattr_t* attr);

// Destroy attribute object `attr'.
extern int bthread_rwlockattr_destroy(bthread_rwlockattr_t* attr);

// Return current setting of reader/writer preference.
extern int bthread_rwlockattr_getkind_np(const bthread_rwlockattr_t* attr,
                                         int* pref);

// Set reader/write preference.
extern int bthread_rwlockattr_setkind_np(bthread_rwlockattr_t* attr,
                                         int pref);

// -------------------------------------------
// Functions for handling semaphore.
// -------------------------------------------

// Initialize the semaphore referred to by `sem'. The value of the
// initialized semaphore shall be `value'.
// Return 0 on success, errno otherwise.
extern int bthread_sem_init(bthread_sem_t* sem, unsigned value);

// Disable the contention profile of the semaphore  referred to by `sem'.
extern int bthread_sem_disable_csite(bthread_sem_t* sem);

// Destroy the semaphore indicated by `sem'.
// Return 0 on success, errno otherwise.
extern int bthread_sem_destroy(bthread_sem_t* semaphore);

// Lock the semaphore referenced by `sem' by performing a semaphore
// lock operation on that semaphore. If the semaphore value is currently
// zero, then the calling (b)thread shall not return from the call to
// bthread_sema_wait() function until it locks the semaphore.
// Return 0 on success, errno otherwise.
extern int bthread_sem_wait(bthread_sem_t* sem);

// Lock the semaphore referenced by `sem' as in the bthread_sem_wait()
// function. However, if the semaphore cannot be locked without waiting
// for another (b)thread to unlock the semaphore by performing a
// bthread_sem_post() function, this wait shall be terminated when the
// specified timeout expires.
// Return 0 on success, errno otherwise.
extern int bthread_sem_timedwait(bthread_sem_t* sem, const struct timespec* abstime);

// Lock the semaphore referenced by `sem' only if the semaphore is
// currently not locked; that is, if the semaphore value is currently
// positive. Otherwise, it shall not lock the semaphore.
// Return 0 on success, errno otherwise.
extern int bthread_sem_trywait(bthread_sem_t* sem);

// Unlock the semaphore referenced by `sem' by performing
// a semaphore unlock operation on that semaphore.
// Return 0 on success, errno otherwise.
extern int bthread_sem_post(bthread_sem_t* sem);

// Unlock the semaphore referenced by `sem' by performing
// `n' semaphore unlock operation on that semaphore.
// Return 0 on success, errno otherwise.
extern int bthread_sem_post_n(bthread_sem_t* sem, size_t n);


// ----------------------------------------------------------------------
// Functions for handling barrier which is a new feature in 1003.1j-2000.
// ----------------------------------------------------------------------

extern int bthread_barrier_init(bthread_barrier_t* __restrict barrier,
                                const bthread_barrierattr_t* __restrict attr,
                                unsigned count);

extern int bthread_barrier_destroy(bthread_barrier_t* barrier);

extern int bthread_barrier_wait(bthread_barrier_t* barrier);

// ---------------------------------------------------------------------
// Functions for handling thread-specific data. 
// Notice that they can be used in pthread: get pthread-specific data in 
// pthreads and get bthread-specific data in bthreads.
// ---------------------------------------------------------------------

// Create a key value identifying a slot in a thread-specific data area.
// Each thread maintains a distinct thread-specific data area.
// `destructor', if non-NULL, is called with the value associated to that key
// when the key is destroyed. `destructor' is not called if the value
// associated is NULL when the key is destroyed.
// Returns 0 on success, error code otherwise.
extern int bthread_key_create(bthread_key_t* key,
                              void (*destructor)(void* data));

// Delete a key previously returned by bthread_key_create().
// It is the responsibility of the application to free the data related to
// the deleted key in any running thread. No destructor is invoked by
// this function. Any destructor that may have been associated with key
// will no longer be called upon thread exit.
// Returns 0 on success, error code otherwise.
extern int bthread_key_delete(bthread_key_t key);

// Store `data' in the thread-specific slot identified by `key'.
// bthread_setspecific() is callable from within destructor. If the application
// does so, destructors will be repeatedly called for at most
// PTHREAD_DESTRUCTOR_ITERATIONS times to clear the slots.
// NOTE: If the thread is not created by brpc server and lifetime is
// very short(doing a little thing and exit), avoid using bthread-local. The
// reason is that bthread-local always allocate keytable on first call to 
// bthread_setspecific, the overhead is negligible in long-lived threads,
// but noticeable in shortly-lived threads. Threads in brpc server
// are special since they reuse keytables from a bthread_keytable_pool_t
// in the server.
// Returns 0 on success, error code otherwise.
// If the key is invalid or deleted, return EINVAL.
extern int bthread_setspecific(bthread_key_t key, void* data);

// Return current value of the thread-specific slot identified by `key'.
// If bthread_setspecific() had not been called in the thread, return NULL.
// If the key is invalid or deleted, return NULL.
extern void* bthread_getspecific(bthread_key_t key);

// Return current bthread tag
extern bthread_tag_t bthread_self_tag(void);

// The first call to bthread_once() by any thread in a process, with a given
// once_control, will call the init_routine() with no arguments. Subsequent
// calls of bthread_once() with the same once_control will not call the
// init_routine(). On return from bthread_once(), it is guaranteed that
// init_routine() has completed. The once_control parameter is used to
// determine whether the associated initialisation routine has been called.
// Returns 0 on success, error code otherwise.
extern int bthread_once(bthread_once_t* once_control, void (*init_routine)());

 /**
 * @brief Retrieves the CPU time consumed by the current bthread.
 *
 * This function returns the CPU time (in nanoseconds) used by the current 
 * bthread, excluding time spent in blocking I/O operations. The result 
 * provides an accurate measure of CPU time utilized by the bthread's 
 * execution.
 *
 * @note The functionality of this function depends on the 
 *       `bthread_enable_cpu_clock_stat` flag. Ensure this flag is enabled 
 *       for the function to provide meaningful results. If the flag is 
 *       disabled, the function may return an invalid value or behave 
 *       unexpectedly.
 *
 * @return int64_t The CPU time in nanoseconds consumed by the bthread.
 */
extern uint64_t bthread_cpu_clock_ns(void);

__END_DECLS

#endif  // BTHREAD_BTHREAD_H
