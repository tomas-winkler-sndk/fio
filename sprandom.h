/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */

#ifndef FIO_SPRANDOM_H
#define FIO_SPRANDOM_H

#include <pthread.h>
#include "fio.h"

/**
 * struct sprandom_info - Structure holding information for sparse random operations.
 * @mutex:           Mutex to protect access to refcount
 * @refcount:        Reference count for managing the lifetime of the structure.
 * @over_provision:  Over-provisioning ratio for the flash device
 * @region_sz:       Size of each region in bytes.
 * @nregions:        Number of regions managed by this structure.
 * @offsets:         Flexible array of offsets for each region.
 *
 * This structure is used to manage sparse random regions.
 */
struct sprandom_info {
    pthread_mutex_t    mutex;
    uint32_t           refcount;
    double             over_provision;
    uint64_t           region_sz;
    uint32_t           nregions;
    uint64_t           offsets[];
};

/**
 * sprandom_init_files - Initialize sprandom-related data for a thread.
 * @td: Pointer to the thread_data structure.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int sprandom_init_files(struct thread_data *td);
/**
 * sprandom_free_info - Free sprandom-related resources for a file.
 * @f: Pointer to the fio_file structure representing the file.
 */

void sprandom_free_info(struct fio_file *f);
/**
 * sprandom_io_size - Get the I/O size for sprandom operations on a file.
 * @f: Pointer to the fio_file structure representing the file.
 *
 * Returns: The I/O size in bytes as a uint64_t value.
 */
uint64_t sprandom_io_size(const struct fio_file *f);

#endif /* FIO_SPRANDOM_H */
