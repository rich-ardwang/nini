/* This file implements atomic counters using __atomic or __sync macros if
 * available, otherwise synchronizing different threads using a mutex.
 *
 * The exported interface is composed of three macros:
 *
 * atomicIncr(var,count) -- Increment the atomic counter
 * atomicGetIncr(var,oldvalue_var,count) -- Get and increment the atomic counter
 * atomicDecr(var,count) -- Decrement the atomic counter
 * atomicGet(var,dstvar) -- Fetch the atomic counter value
 * atomicSet(var,value)  -- Set the atomic counter value
 *
 * The variable 'var' should also have a declared mutex with the same
 * name and the "_mutex" postfix, for instance:
 *
 *  long myvar;
 *  pthread_mutex_t myvar_mutex;
 *  atomicSet(myvar,12345);
 *
 * If atomic primitives are available (tested in config.h) the mutex
 * is not used.
 *
 * Never use return value from the macros, instead use the AtomicGetIncr()
 * if you need to get the current value and increment it atomically, like
 * in the followign example:
 *
 *  long oldvalue;
 *  atomicGetIncr(myvar,oldvalue,1);
 *  doSomethingWith(oldvalue);
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#include <pthread.h>

#ifndef _NI_ATOMIC_H_
#define _NI_ATOMIC_H_

#define atomicIncr(var,count) do { \
    pthread_mutex_lock(&var ## _mutex); \
    var += (count); \
    pthread_mutex_unlock(&var ## _mutex); \
} while(0)
#define atomicGetIncr(var,oldvalue_var,count) do { \
    pthread_mutex_lock(&var ## _mutex); \
    oldvalue_var = var; \
    var += (count); \
    pthread_mutex_unlock(&var ## _mutex); \
} while(0)
#define atomicDecr(var,count) do { \
    pthread_mutex_lock(&var ## _mutex); \
    var -= (count); \
    pthread_mutex_unlock(&var ## _mutex); \
} while(0)
#define atomicGet(var,dstvar) do { \
    pthread_mutex_lock(&var ## _mutex); \
    dstvar = var; \
    pthread_mutex_unlock(&var ## _mutex); \
} while(0)
#define atomicSet(var,value) do { \
    pthread_mutex_lock(&var ## _mutex); \
    var = value; \
    pthread_mutex_unlock(&var ## _mutex); \
} while(0)

#endif /* _NI_ATOMIC_H_ */
