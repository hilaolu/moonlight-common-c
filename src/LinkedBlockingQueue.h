#pragma once

#include "Platform.h"
#include "PlatformThreads.h"

#define LBQ_SUCCESS 0
#define LBQ_INTERRUPTED 1
#define LBQ_BOUND_EXCEEDED 2
#define LBQ_NO_ELEMENT 3
#define LBQ_USER_WAKE 4
#define LBQ_MERGED 5

typedef bool (*PLBQ_TAIL_MERGE_CALLBACK)(void* tailData, void* offeredData, void* context);
typedef bool (*PLBQ_EVICTION_PREDICATE)(void* data, void* context);
typedef bool (*PLBQ_POLL_PREDICATE)(void* data, void* context);

#define LBQ_RESIDENCE_TIME_BUCKET_COUNT 66

typedef struct _LINKED_BLOCKING_QUEUE_STATS {
    int currentDepth;
    int maximumDepth;
    uint64_t enqueuedItems;
    uint64_t dequeuedItems;
    uint64_t tailMerges;
    uint64_t oldestEvictions;
    uint64_t boundRejections;
    uint64_t residenceSampleCount;
    uint64_t p50ResidenceTimeUs;
    uint64_t p95ResidenceTimeUs;
    uint64_t p99ResidenceTimeUs;
    uint64_t maximumResidenceTimeUs;
} LINKED_BLOCKING_QUEUE_STATS, *PLINKED_BLOCKING_QUEUE_STATS;

typedef struct _LINKED_BLOCKING_QUEUE_ENTRY {
    struct _LINKED_BLOCKING_QUEUE_ENTRY* flink;
    struct _LINKED_BLOCKING_QUEUE_ENTRY* blink;
    void* data;
} LINKED_BLOCKING_QUEUE_ENTRY, *PLINKED_BLOCKING_QUEUE_ENTRY;

typedef struct _LINKED_BLOCKING_QUEUE {
    PLT_MUTEX mutex;
    PLT_COND cond;
    PLINKED_BLOCKING_QUEUE_ENTRY head;
    PLINKED_BLOCKING_QUEUE_ENTRY tail;
    int sizeBound;
    int currentSize;
    int lifetimeSize;
    int maximumSize;
    uint64_t enqueuedItemCount;
    uint64_t dequeuedItemCount;
    uint64_t tailMergeCount;
    uint64_t oldestEvictionCount;
    uint64_t boundRejectionCount;
    uint64_t residenceSampleCount;
    uint64_t maximumResidenceTimeUs;
    uint64_t residenceTimeHistogram[LBQ_RESIDENCE_TIME_BUCKET_COUNT];
    bool shutdown;
    bool draining;
    bool pendingUserWake;
} LINKED_BLOCKING_QUEUE, *PLINKED_BLOCKING_QUEUE;

int LbqInitializeLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead, int sizeBound);
int LbqOfferQueueItem(PLINKED_BLOCKING_QUEUE queueHead, void* data, PLINKED_BLOCKING_QUEUE_ENTRY entry);
int LbqOfferQueueItemWithTailMerge(PLINKED_BLOCKING_QUEUE queueHead, void* data,
                                   PLINKED_BLOCKING_QUEUE_ENTRY entry,
                                   PLBQ_TAIL_MERGE_CALLBACK mergeCallback, void* context);
int LbqOfferQueueItemWithOldestEviction(PLINKED_BLOCKING_QUEUE queueHead, void* data,
                                        PLINKED_BLOCKING_QUEUE_ENTRY entry,
                                        PLBQ_EVICTION_PREDICATE evictionPredicate,
                                        void* context, void** evictedData);
int LbqWaitForQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data);
int LbqPollQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data);
int LbqPollQueueElementIf(PLINKED_BLOCKING_QUEUE queueHead,
                          PLBQ_POLL_PREDICATE predicate,
                          void* context,
                          void** data);
int LbqPeekQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data);
PLINKED_BLOCKING_QUEUE_ENTRY LbqDestroyLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead);
PLINKED_BLOCKING_QUEUE_ENTRY LbqFlushQueueItems(PLINKED_BLOCKING_QUEUE queueHead);
void LbqSignalQueueShutdown(PLINKED_BLOCKING_QUEUE queueHead);
void LbqSignalQueueDrain(PLINKED_BLOCKING_QUEUE queueHead);
void LbqSignalQueueUserWake(PLINKED_BLOCKING_QUEUE queueHead);
int LbqGetItemCount(PLINKED_BLOCKING_QUEUE queueHead);
void LbqRecordQueueResidenceTimeUs(PLINKED_BLOCKING_QUEUE queueHead, uint64_t residenceTimeUs);
void LbqGetQueueStats(PLINKED_BLOCKING_QUEUE queueHead, PLINKED_BLOCKING_QUEUE_STATS stats);
