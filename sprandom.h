/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */

#ifndef FIO_SPRANDOM_H
#define FIO_SPRANDOM_H

#include <stdint.h>
#include "lib/rand.h"

/**
 * struct sprandom_info - Structure holding information for sparse random operations.
 * @over_provision:  Over-provisioning ratio for the flash device
 * @region_sz:       Size of each region in bytes.
 * @nregions:        Number of regions managed by this structure.
 * @offsets:         Flexible array of offsets for each region.
 *
 * This structure is used to manage sparse random regions.
 */
struct sprandom_info {
	double                 over_provision;
	uint64_t               region_sz;
	uint64_t               wr_remaining;
	uint32_t               curr_region;
	uint32_t               num_regions;
	double                 *validity_dist;
	int                     *invalid_pct;

	uint64_t total_lbas;
	uint64_t region_write_count;


	// Invalidation list
	uint64_t *invalid_lbas[2];
	size_t    invalid_count[2];
	size_t    invalid_index[2];

	size_t    invalid_capacity;
	uint32_t  curr_phase;


	// Region and write tracking
	uint32_t current_region;
	uint64_t writes_remaining;

	struct frand_state rand_state;
};

#endif /* FIO_SPRANDOM_H */

int sprandom_init(struct thread_data *td, struct fio_file *f);

void sprandom_free(struct sprandom_info *info);

int sprandom_get_next_offset(struct sprandom_info *info, struct fio_file *f, uint64_t *b);
