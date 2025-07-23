/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */
#ifndef PHASE_CIRCULAR_BUFFER_H
#define PHASE_CIRCULAR_BUFFER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

struct pc_buf {
    uint64_t commit_head;
    uint64_t staging_head;
    uint64_t read_tail;
    uint64_t capacity;
    uint64_t buffer[];
};

static inline struct pc_buf* pcb_alloc(uint64_t capacity)
{
    size_t size = sizeof(struct pc_buf) + sizeof(uint64_t) * capacity;
    struct pc_buf *cb = (struct pc_buf *)malloc(size);

    if (!cb)
	    return NULL;
    cb->commit_head = 0;
    cb->staging_head = 0;
    cb->read_tail = 0;
    cb->capacity = capacity;
    return cb;
}

static inline bool pcb_is_empty(struct pc_buf *cb)
{
    return cb->read_tail == cb->commit_head;
}

static inline bool pcb_is_full(struct pc_buf *cb)
{
    return ((cb->staging_head + 1) % cb->capacity) == cb->read_tail;
}

static inline bool pcb_push_staged(struct pc_buf *cb, uint64_t value)
{
    if (pcb_is_full(cb)) {
        return false;
    }
    cb->buffer[cb->staging_head] = value;
    cb->staging_head = (cb->staging_head + 1) % cb->capacity;
    return true;
}

static inline void pcb_commit(struct pc_buf *cb) {
    cb->commit_head = cb->staging_head;
}

static inline bool pcb_pop(struct pc_buf *cb, uint64_t *out)
{
    if (pcb_is_empty(cb)) {
        return false;
    }
    *out = cb->buffer[cb->read_tail];
    cb->read_tail = (cb->read_tail + 1) % cb->capacity;
    return true;
}

static inline void pcb_print_visible(struct pc_buf *cb)
{
    uint64_t i = cb->read_tail;
    printf("Visible buffer: ");
    while (i != cb->commit_head) {
        printf("%" PRIu64 " ", cb->buffer[i]);
        i = (i + 1) % cb->capacity;
    }
    printf("\n");
}

static inline void pcb_print_staged(struct pc_buf *cb)
{
    uint64_t i = cb->commit_head;
    printf("Staged (not visible yet): ");
    while (i != cb->staging_head) {
        printf("%" PRIu64 " ", cb->buffer[i]);
        i = (i + 1) % cb->capacity;
    }
    printf("\n");
}

static inline uint64_t pcb_visible_size(struct pc_buf *cb)
{
    if (cb->commit_head >= cb->read_tail)
        return cb->commit_head - cb->read_tail;
    else
        return cb->capacity - cb->read_tail + cb->commit_head;
}

static inline uint64_t pcb_staged_size(struct pc_buf *cb)
{
    if (cb->staging_head >= cb->commit_head)
        return cb->staging_head - cb->commit_head;
    else
        return cb->capacity - cb->commit_head + cb->staging_head;
}

static inline bool pcb_space_available(struct pc_buf *cb)
{
    uint64_t used = pcb_visible_size(cb) + pcb_staged_size(cb);
    /* keep 1 slot reserved to distinguish full from empty */
    return used < (cb->capacity - 1);
}

#endif /* PHASE_CIRCULAR_BUFFER_H */

