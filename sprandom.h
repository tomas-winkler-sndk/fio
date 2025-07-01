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
	uint32_t           nregions;
	uint64_t           offsets[];
};

#endif /* FIO_SPRANDOM_H */
