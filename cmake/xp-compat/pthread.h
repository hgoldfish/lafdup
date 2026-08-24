/*
 * Public domain
 * pthread.h compatibility shim — Windows XP variant.
 *
 * Upstream LibreSSL uses InitOnceExecuteOnce (Vista+). This copy uses
 * InterlockedCompareExchange so XP can load the binary.
 */

#ifndef LIBCRYPTOCOMPAT_PTHREAD_H
#define LIBCRYPTOCOMPAT_PTHREAD_H

#ifdef _WIN32

#include <malloc.h>
#include <stdlib.h>
#include <windows.h>

#define PTHREAD_ONCE_INIT \
    {                     \
        0, 0              \
    }

#define PTHREAD_MUTEX_INITIALIZER \
    {                             \
        .lock = NULL              \
    }

struct pthread_once {
    LONG started;
    LONG done;
};
typedef struct pthread_once pthread_once_t;

static inline int
pthread_once(pthread_once_t *once, void (*cb)(void))
{
    if (once->done) {
        return 0;
    }
    if (InterlockedCompareExchange(&once->started, 1, 0) == 0) {
        cb();
        MemoryBarrier();
        once->done = 1;
    } else {
        while (!once->done) {
            Sleep(0);
        }
    }
    return 0;
}

typedef DWORD pthread_t;

static inline pthread_t
pthread_self(void)
{
    return GetCurrentThreadId();
}

static inline int
pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

struct pthread_mutex {
    volatile LPCRITICAL_SECTION lock;
};
typedef struct pthread_mutex pthread_mutex_t;
typedef void pthread_mutexattr_t;

static inline int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void)attr;
    if ((mutex->lock = malloc(sizeof(CRITICAL_SECTION))) == NULL)
        exit(ENOMEM);
    InitializeCriticalSection(mutex->lock);
    return 0;
}

static inline int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex->lock == NULL) {
        LPCRITICAL_SECTION lcs;

        if ((lcs = malloc(sizeof(CRITICAL_SECTION))) == NULL)
            exit(ENOMEM);
        InitializeCriticalSection(lcs);
        if (InterlockedCompareExchangePointer((PVOID *)&mutex->lock, (PVOID)lcs, NULL) != NULL) {
            DeleteCriticalSection(lcs);
            free(lcs);
        }
    }
    EnterCriticalSection(mutex->lock);
    return 0;
}

static inline int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    LeaveCriticalSection(mutex->lock);
    return 0;
}

static inline int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    DeleteCriticalSection(mutex->lock);
    free(mutex->lock);
    return 0;
}

#else
#include_next <pthread.h>
#endif

#endif
