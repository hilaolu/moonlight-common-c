#pragma once

#include "Limelight.h"
#include "Platform.h"

typedef void(*ThreadEntry)(void* context);

// Cross-thread booleans must be accessed through these helpers. In particular,
// volatile is not sufficient to publish state on weakly ordered CPUs such as
// ARM64.
typedef struct _PLT_ATOMIC_BOOL {
#if defined(LC_WINDOWS)
    LONG value;
#else
    // Use a naturally aligned word so the operation stays lock-free on all of
    // our GCC/Clang targets, including older console toolchains.
    int value;
#endif
} PLT_ATOMIC_BOOL;

#define PLT_ATOMIC_BOOL_INITIALIZER(value) { (value) }

static inline bool PltAtomicLoadBoolAcquire(PLT_ATOMIC_BOOL* atomic) {
#if defined(LC_WINDOWS)
    // MSVC's Interlocked operations are full barriers, which is stronger than
    // the acquire ordering required here.
    return InterlockedCompareExchange(&atomic->value, 0, 0) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(&atomic->value, __ATOMIC_ACQUIRE) != 0;
#else
#error Atomic boolean operations are not implemented for this compiler
#endif
}

static inline void PltAtomicStoreBoolRelease(PLT_ATOMIC_BOOL* atomic, bool newValue) {
#if defined(LC_WINDOWS)
    // MSVC's Interlocked operations are full barriers, which is stronger than
    // the release ordering required here.
    InterlockedExchange(&atomic->value, newValue ? 1 : 0);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(&atomic->value, newValue, __ATOMIC_RELEASE);
#else
#error Atomic boolean operations are not implemented for this compiler
#endif
}

static inline bool PltAtomicLoadBoolRelaxed(PLT_ATOMIC_BOOL* atomic) {
#if defined(LC_WINDOWS)
    return InterlockedCompareExchange(&atomic->value, 0, 0) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(&atomic->value, __ATOMIC_RELAXED) != 0;
#else
#error Atomic boolean operations are not implemented for this compiler
#endif
}

static inline void PltAtomicStoreBoolRelaxed(PLT_ATOMIC_BOOL* atomic, bool newValue) {
#if defined(LC_WINDOWS)
    InterlockedExchange(&atomic->value, newValue ? 1 : 0);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(&atomic->value, newValue, __ATOMIC_RELAXED);
#else
#error Atomic boolean operations are not implemented for this compiler
#endif
}

#if defined(LC_WINDOWS)
typedef SRWLOCK PLT_MUTEX;
typedef CONDITION_VARIABLE PLT_COND;
typedef struct _PLT_THREAD {
    HANDLE handle;
    PLT_ATOMIC_BOOL cancelled;
} PLT_THREAD;
#elif defined(__WIIU__)
typedef OSFastMutex PLT_MUTEX;
typedef OSFastCondition PLT_COND;
typedef struct _PLT_THREAD {
    OSThread thread;
    PLT_ATOMIC_BOOL cancelled;
} PLT_THREAD;
#elif defined(__3DS__)
typedef LightLock PLT_MUTEX;
typedef CondVar PLT_COND;
typedef struct _PLT_THREAD {
    Thread thread;
    PLT_ATOMIC_BOOL cancelled;
} PLT_THREAD;
#elif defined (LC_POSIX)
typedef pthread_mutex_t PLT_MUTEX;
typedef pthread_cond_t PLT_COND;
typedef struct _PLT_THREAD {
    pthread_t thread;
    PLT_ATOMIC_BOOL cancelled;
} PLT_THREAD;
#else
#error Unsupported platform
#endif

#ifdef LC_WINDOWS
typedef HANDLE PLT_EVENT;
#else
typedef struct _PLT_EVENT {
    PLT_MUTEX mutex;
    PLT_COND cond;
    bool signalled;
} PLT_EVENT;
#endif

int PltCreateMutex(PLT_MUTEX* mutex);
void PltDeleteMutex(PLT_MUTEX* mutex);
void PltLockMutex(PLT_MUTEX* mutex);
void PltUnlockMutex(PLT_MUTEX* mutex);

int PltCreateThread(const char* name, ThreadEntry entry, void* context, PLT_THREAD* thread);
void PltInterruptThread(PLT_THREAD* thread);
bool PltIsThreadInterrupted(PLT_THREAD* thread);
void PltJoinThread(PLT_THREAD* thread);
void PltDetachThread(PLT_THREAD* thread);

int PltCreateEvent(PLT_EVENT* event);
void PltCloseEvent(PLT_EVENT* event);
void PltSetEvent(PLT_EVENT* event);
void PltClearEvent(PLT_EVENT* event);
void PltWaitForEvent(PLT_EVENT* event);

int PltCreateConditionVariable(PLT_COND* cond, PLT_MUTEX* mutex);
void PltDeleteConditionVariable(PLT_COND* cond);
void PltSignalConditionVariable(PLT_COND* cond);
void PltWaitForConditionVariable(PLT_COND* cond, PLT_MUTEX* mutex);

void PltSleepMs(int ms);
void PltSleepMsInterruptible(PLT_THREAD* thread, int ms);
