/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */

#ifndef FIO_SPRANDOM_H
#define FIO_SPRANDOM_H

#include <stdint.h>
#include "lib/rand.h"
#include "pcbuf.h"

/**
 * struct sprandom_info - information for sprandom operations.
 *
 * @over_provision:  Over-provisioning ratio for the flash device.
 * @region_sz:       Size of each region in bytes.
 * @num_regions:     Number of SPRandom regions.
 */
struct sprandom_info {
	double    over_provision;
	uint64_t  region_sz;
	uint32_t  num_regions;
	uint64_t  wr_remaining;
	uint32_t  curr_region;

	double    *validity_dist;
	uint32_t  *invalid_pct;

	/* Invalidation list*/
	struct pc_buf *invalid_buf;
	size_t    invalid_count[2];
	size_t    invalid_capacity;
	uint32_t  curr_phase;

	/* Region and write tracking */
	uint64_t region_write_count;
	uint32_t current_region;
	uint64_t writes_remaining;

	struct frand_state rand_state;
};

#endif /* FIO_SPRANDOM_H */

int sprandom_init(struct thread_data *td, struct fio_file *f);

void sprandom_free(struct sprandom_info *info);

int sprandom_get_next_offset(struct sprandom_info *info, struct fio_file *f, uint64_t *b);
