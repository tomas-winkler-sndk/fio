/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */

#ifndef FIO_SPRANDOM_H
#define FIO_SPRANDOM_H

#include "fio.h"
#include "lib/lfsr.h"

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
	double             over_provision;
	uint64_t           region_sz;
	uint64_t           region_blocks;
	uint32_t           nregions;
	uint32_t           region_current;
	struct {
		double           validity;
		struct fio_lfsr  lfrs_state;
	} regions[];
};

int sprandom_create_info(struct thread_data *td, struct fio_file *f);

#endif /* FIO_SPRANDOM_H */
