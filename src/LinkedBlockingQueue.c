#include "LinkedBlockingQueue.h"

static int getResidenceTimeBucket(uint64_t residenceTimeUs) {
    int bucket = 1;
    uint64_t upperBound = 1;

    if (residenceTimeUs == 0) {
        return 0;
    }

    while (residenceTimeUs > upperBound && bucket < LBQ_RESIDENCE_TIME_BUCKET_COUNT - 2) {
        upperBound <<= 1;
        bucket++;
    }

    if (residenceTimeUs > upperBound) {
        return LBQ_RESIDENCE_TIME_BUCKET_COUNT - 1;
    }

    return bucket;
}

static uint64_t getResidenceTimeBucketUpperBound(int bucket) {
    if (bucket == 0) {
        return 0;
    }
    if (bucket == LBQ_RESIDENCE_TIME_BUCKET_COUNT - 1) {
        return UINT64_MAX;
    }

    return (uint64_t)1 << (bucket - 1);
}

static uint64_t getResidenceTimePercentile(PLINKED_BLOCKING_QUEUE queueHead, int percentile) {
    uint64_t cumulativeSamples = 0;
    uint64_t targetSample;
    int i;

    if (queueHead->residenceSampleCount == 0) {
        return 0;
    }

    targetSample = (queueHead->residenceSampleCount / 100) * percentile;
    targetSample += ((queueHead->residenceSampleCount % 100) * percentile + 99) / 100;

    for (i = 0; i < LBQ_RESIDENCE_TIME_BUCKET_COUNT; i++) {
        cumulativeSamples += queueHead->residenceTimeHistogram[i];
        if (cumulativeSamples >= targetSample) {
            return getResidenceTimeBucketUpperBound(i);
        }
    }

    LC_ASSERT(false);
    return queueHead->maximumResidenceTimeUs;
}

// Destroy the linked blocking queue and associated mutex and event
PLINKED_BLOCKING_QUEUE_ENTRY LbqDestroyLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead) {
    LC_ASSERT(queueHead->shutdown || queueHead->draining || queueHead->lifetimeSize == 0);
    
    PltDeleteMutex(&queueHead->mutex);
    PltDeleteConditionVariable(&queueHead->cond);

    return queueHead->head;
}

// Flush the queue
PLINKED_BLOCKING_QUEUE_ENTRY LbqFlushQueueItems(PLINKED_BLOCKING_QUEUE queueHead) {
    PLINKED_BLOCKING_QUEUE_ENTRY head;

    PltLockMutex(&queueHead->mutex);

    // Save the old head
    head = queueHead->head;

    // Reinitialize the queue to empty
    if (head != NULL) {
        queueHead->head = NULL;
        queueHead->tail = NULL;
        queueHead->currentSize = 0;
    }
    else {
        LC_ASSERT(queueHead->tail == NULL);
        LC_ASSERT(queueHead->currentSize == 0);
    }

    PltUnlockMutex(&queueHead->mutex);

    return head;
}

// Linked blocking queue init
int LbqInitializeLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead, int sizeBound) {
    int err;

    memset(queueHead, 0, sizeof(*queueHead));

    err = PltCreateMutex(&queueHead->mutex);
    if (err != 0) {
        return err;
    }

    err = PltCreateConditionVariable(&queueHead->cond, &queueHead->mutex);
    if (err != 0) {
        PltDeleteMutex(&queueHead->mutex);
        return err;
    }

    queueHead->sizeBound = sizeBound;

    return 0;
}

void LbqSignalQueueShutdown(PLINKED_BLOCKING_QUEUE queueHead) {
    PltLockMutex(&queueHead->mutex);
    queueHead->shutdown = true;
    PltUnlockMutex(&queueHead->mutex);
    PltSignalConditionVariable(&queueHead->cond);
}

void LbqSignalQueueDrain(PLINKED_BLOCKING_QUEUE queueHead) {
    PltLockMutex(&queueHead->mutex);
    queueHead->draining = true;
    PltUnlockMutex(&queueHead->mutex);
    PltSignalConditionVariable(&queueHead->cond);
}

void LbqSignalQueueUserWake(PLINKED_BLOCKING_QUEUE queueHead) {
    PltLockMutex(&queueHead->mutex);
    queueHead->pendingUserWake = true;
    PltUnlockMutex(&queueHead->mutex);
    PltSignalConditionVariable(&queueHead->cond);
}

int LbqGetItemCount(PLINKED_BLOCKING_QUEUE queueHead) {
    int itemCount;

    PltLockMutex(&queueHead->mutex);
    itemCount = queueHead->currentSize;
    PltUnlockMutex(&queueHead->mutex);

    return itemCount;
}

void LbqRecordQueueResidenceTimeUs(PLINKED_BLOCKING_QUEUE queueHead, uint64_t residenceTimeUs) {
    int bucket = getResidenceTimeBucket(residenceTimeUs);

    PltLockMutex(&queueHead->mutex);
    queueHead->residenceTimeHistogram[bucket]++;
    queueHead->residenceSampleCount++;
    if (residenceTimeUs > queueHead->maximumResidenceTimeUs) {
        queueHead->maximumResidenceTimeUs = residenceTimeUs;
    }
    PltUnlockMutex(&queueHead->mutex);
}

void LbqGetQueueStats(PLINKED_BLOCKING_QUEUE queueHead, PLINKED_BLOCKING_QUEUE_STATS stats) {
    LC_ASSERT(stats != NULL);

    memset(stats, 0, sizeof(*stats));

    PltLockMutex(&queueHead->mutex);
    stats->currentDepth = queueHead->currentSize;
    stats->maximumDepth = queueHead->maximumSize;
    stats->enqueuedItems = queueHead->enqueuedItemCount;
    stats->dequeuedItems = queueHead->dequeuedItemCount;
    stats->tailMerges = queueHead->tailMergeCount;
    stats->oldestEvictions = queueHead->oldestEvictionCount;
    stats->boundRejections = queueHead->boundRejectionCount;
    stats->residenceSampleCount = queueHead->residenceSampleCount;
    stats->p50ResidenceTimeUs = getResidenceTimePercentile(queueHead, 50);
    stats->p95ResidenceTimeUs = getResidenceTimePercentile(queueHead, 95);
    stats->p99ResidenceTimeUs = getResidenceTimePercentile(queueHead, 99);
    stats->maximumResidenceTimeUs = queueHead->maximumResidenceTimeUs;
    PltUnlockMutex(&queueHead->mutex);
}

int LbqOfferQueueItemWithTailMerge(PLINKED_BLOCKING_QUEUE queueHead, void* data,
                                   PLINKED_BLOCKING_QUEUE_ENTRY entry,
                                   PLBQ_TAIL_MERGE_CALLBACK mergeCallback, void* context) {
    bool wasEmpty;
    
    entry->flink = NULL;
    entry->data = data;

    PltLockMutex(&queueHead->mutex);

    if (queueHead->shutdown || queueHead->draining) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    // The callback executes while holding the queue mutex, so the consumer
    // cannot dequeue or free the tail while it is being inspected or updated.
    if (queueHead->tail != NULL && mergeCallback != NULL &&
            mergeCallback(queueHead->tail->data, data, context)) {
        queueHead->tailMergeCount++;
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_MERGED;
    }

    if (queueHead->currentSize == queueHead->sizeBound) {
        queueHead->boundRejectionCount++;
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_BOUND_EXCEEDED;
    }

    wasEmpty = queueHead->head == NULL;
    if (wasEmpty) {
        LC_ASSERT(queueHead->currentSize == 0);
        LC_ASSERT(queueHead->tail == NULL);
        queueHead->head = entry;
        queueHead->tail = entry;
        entry->blink = NULL;
    }
    else {
        LC_ASSERT(queueHead->currentSize >= 1);
        LC_ASSERT(queueHead->head != NULL);
        queueHead->tail->flink = entry;
        entry->blink = queueHead->tail;
        queueHead->tail = entry;
    }

    queueHead->currentSize++;
    queueHead->lifetimeSize++;
    queueHead->enqueuedItemCount++;
    if (queueHead->currentSize > queueHead->maximumSize) {
        queueHead->maximumSize = queueHead->currentSize;
    }

    PltUnlockMutex(&queueHead->mutex);

    if (wasEmpty) {
        // Only call PltSignalConditionVariable() when transitioning from
        // empty -> non-empty to avoid a useless syscall for each new entry.
        PltSignalConditionVariable(&queueHead->cond);
    }

    return LBQ_SUCCESS;
}

int LbqOfferQueueItem(PLINKED_BLOCKING_QUEUE queueHead, void* data, PLINKED_BLOCKING_QUEUE_ENTRY entry) {
    return LbqOfferQueueItemWithTailMerge(queueHead, data, entry, NULL, NULL);
}

int LbqOfferQueueItemWithOldestEviction(PLINKED_BLOCKING_QUEUE queueHead, void* data,
                                        PLINKED_BLOCKING_QUEUE_ENTRY entry,
                                        PLBQ_EVICTION_PREDICATE evictionPredicate,
                                        void* context, void** evictedData) {
    PLINKED_BLOCKING_QUEUE_ENTRY evictedEntry = NULL;
    bool wasEmpty;

    LC_ASSERT(evictedData != NULL);
    *evictedData = NULL;
    entry->flink = NULL;
    entry->data = data;

    PltLockMutex(&queueHead->mutex);

    if (queueHead->shutdown || queueHead->draining) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    wasEmpty = queueHead->head == NULL;
    if (queueHead->currentSize == queueHead->sizeBound) {
        for (evictedEntry = queueHead->head; evictedEntry != NULL; evictedEntry = evictedEntry->flink) {
            if (evictionPredicate != NULL && evictionPredicate(evictedEntry->data, context)) {
                break;
            }
        }

        if (evictedEntry == NULL) {
            queueHead->boundRejectionCount++;
            PltUnlockMutex(&queueHead->mutex);
            return LBQ_BOUND_EXCEEDED;
        }

        if (evictedEntry->blink != NULL) {
            evictedEntry->blink->flink = evictedEntry->flink;
        }
        else {
            queueHead->head = evictedEntry->flink;
        }

        if (evictedEntry->flink != NULL) {
            evictedEntry->flink->blink = evictedEntry->blink;
        }
        else {
            queueHead->tail = evictedEntry->blink;
        }

        queueHead->currentSize--;
        queueHead->oldestEvictionCount++;
        *evictedData = evictedEntry->data;
        evictedEntry->flink = NULL;
        evictedEntry->blink = NULL;
    }

    if (queueHead->head == NULL) {
        LC_ASSERT(queueHead->currentSize == 0);
        LC_ASSERT(queueHead->tail == NULL);
        queueHead->head = entry;
        queueHead->tail = entry;
        entry->blink = NULL;
    }
    else {
        LC_ASSERT(queueHead->currentSize >= 1);
        LC_ASSERT(queueHead->tail != NULL);
        queueHead->tail->flink = entry;
        entry->blink = queueHead->tail;
        queueHead->tail = entry;
    }

    queueHead->currentSize++;
    queueHead->lifetimeSize++;
    queueHead->enqueuedItemCount++;
    if (queueHead->currentSize > queueHead->maximumSize) {
        queueHead->maximumSize = queueHead->currentSize;
    }

    PltUnlockMutex(&queueHead->mutex);

    if (wasEmpty) {
        PltSignalConditionVariable(&queueHead->cond);
    }

    return LBQ_SUCCESS;
}

// This must be synchronized with LbqFlushQueueItems by the caller
int LbqPeekQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data) {
    PltLockMutex(&queueHead->mutex);

    if (queueHead->shutdown) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    if (queueHead->head == NULL) {
        if (queueHead->draining) {
            PltUnlockMutex(&queueHead->mutex);
            return LBQ_INTERRUPTED;
        }
        else {
            PltUnlockMutex(&queueHead->mutex);
            return LBQ_NO_ELEMENT;
        }
    }

    *data = queueHead->head->data;

    PltUnlockMutex(&queueHead->mutex);

    return LBQ_SUCCESS;
}

int LbqPollQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data) {
    PLINKED_BLOCKING_QUEUE_ENTRY entry;

    PltLockMutex(&queueHead->mutex);

    if (queueHead->shutdown) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    if (queueHead->head == NULL) {
        if (queueHead->draining) {
            PltUnlockMutex(&queueHead->mutex);
            return LBQ_INTERRUPTED;
        }
        else {
            PltUnlockMutex(&queueHead->mutex);
            return LBQ_NO_ELEMENT;
        }
    }

    entry = queueHead->head;
    queueHead->head = entry->flink;
    queueHead->currentSize--;
    queueHead->dequeuedItemCount++;
    if (queueHead->head == NULL) {
        LC_ASSERT(queueHead->currentSize == 0);
        queueHead->tail = NULL;
    }
    else {
        LC_ASSERT(queueHead->currentSize != 0);
        queueHead->head->blink = NULL;
    }

    *data = entry->data;

    PltUnlockMutex(&queueHead->mutex);

    return LBQ_SUCCESS;
}

int LbqPollQueueElementIf(PLINKED_BLOCKING_QUEUE queueHead,
                          PLBQ_POLL_PREDICATE predicate,
                          void* context,
                          void** data) {
    PLINKED_BLOCKING_QUEUE_ENTRY entry;

    PltLockMutex(&queueHead->mutex);

    if (queueHead->shutdown) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    if (queueHead->head == NULL) {
        int err = queueHead->draining ? LBQ_INTERRUPTED : LBQ_NO_ELEMENT;
        PltUnlockMutex(&queueHead->mutex);
        return err;
    }

    // Classification and conditional removal must remain in one critical
    // section. Queue producers can otherwise merge into or evict the head
    // after classification while the consumer still holds its data pointer.
    if (!predicate(queueHead->head->data, context)) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_NO_ELEMENT;
    }

    entry = queueHead->head;
    queueHead->head = entry->flink;
    queueHead->currentSize--;
    queueHead->dequeuedItemCount++;
    if (queueHead->head == NULL) {
        LC_ASSERT(queueHead->currentSize == 0);
        queueHead->tail = NULL;
    }
    else {
        LC_ASSERT(queueHead->currentSize != 0);
        queueHead->head->blink = NULL;
    }

    entry->flink = NULL;
    entry->blink = NULL;
    *data = entry->data;

    PltUnlockMutex(&queueHead->mutex);

    return LBQ_SUCCESS;
}

int LbqWaitForQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data) {
    PLINKED_BLOCKING_QUEUE_ENTRY entry;

    PltLockMutex(&queueHead->mutex);

    // Wait for a waking condition: either data available or rundown
    while (queueHead->head == NULL && !queueHead->draining && !queueHead->shutdown && !queueHead->pendingUserWake) {
        PltWaitForConditionVariable(&queueHead->cond, &queueHead->mutex);
    }

    // If we're shutting down, abort immediately, even if there's data available
    if (queueHead->shutdown) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    // If this is a user requested wake, process it now
    if (queueHead->pendingUserWake) {
        queueHead->pendingUserWake = false;
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_USER_WAKE;
    }

    // If we're draining, only abort if we have no data available
    if (queueHead->draining && queueHead->head == NULL) {
        PltUnlockMutex(&queueHead->mutex);
        return LBQ_INTERRUPTED;
    }

    // We should have bailed by this point if there was no data
    LC_ASSERT(queueHead->head != NULL);

    entry = queueHead->head;
    queueHead->head = entry->flink;
    queueHead->currentSize--;
    queueHead->dequeuedItemCount++;
    if (queueHead->head == NULL) {
        LC_ASSERT(queueHead->currentSize == 0);
        queueHead->tail = NULL;
    }
    else {
        LC_ASSERT(queueHead->currentSize != 0);
        queueHead->head->blink = NULL;
    }

    *data = entry->data;

    PltUnlockMutex(&queueHead->mutex);

    return LBQ_SUCCESS;
}
