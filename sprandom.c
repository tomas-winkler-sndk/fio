/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#ifndef SPRANDOM_UNIT_TEST
#include "fio.h"
#include "file.h"
#endif /* SPRANDOM_UNIT_TESTS */
#include "flist.h"
#include "sprandom.h"
#include "lib/rand.h"

static inline double *d_alloc(size_t n)
{
	return calloc(n, sizeof(double));
}

struct point {
	double x;
	double y;
};

static inline struct point *p_alloc(size_t n)
{
	return calloc(n, sizeof(struct point));
}

static void print_d_array(double *darray, size_t len)
{
	struct buf_output out;
	int i;
	buf_output_init(&out);

	log_buf(&out, "[");
	for (i = 0; i < len - 1; i++) {
		log_buf(&out, "%.2f, ", darray[i]);
	}
	log_buf(&out, "%.2f]\n", darray[len - 1]);
	dprint(FD_SPRANDOM, "%s", out.buf);
	buf_output_free(&out);
}


static const char *human_mem_to_str(char *buf, size_t bufsize, size_t bytes)
{
	const char *units[] = { "B", "KB", "MB", "GB", "TB", "PB" };
	int unit = 0;
	double size = (double)bytes;

	while (size >= 1024.0 && unit < (int)(sizeof(units) / sizeof(units[0])) - 1) {
		size /= 1024.0;
		unit++;
	}

	snprintf(buf, bufsize, "%.2f %s", size, units[unit]);
	return buf;
}
#if 0
static void print_ld_array(uint64_t *ldarray, size_t len)
{
	struct buf_output out;
	unsigned int i;
	buf_output_init(&out);

	log_buf(&out, "[");
	for (i = 0; i < len - 1; i++) {
		log_buf(&out, "%ld, ", ldarray[i]);
	}
	log_buf(&out, "%ld]\n", ldarray[len - 1]);
	dprint(FD_SPRANDOM, "%s", out.buf);
	buf_output_free(&out);
}
#endif

static void print_d_points(struct point *parray, size_t len)
{
	struct buf_output out;
	unsigned int i;
	buf_output_init(&out);

	log_buf(&out, "[");
	for (i = 0; i < len - 1; i++) {
		log_buf(&out, "(%.2f %.2f), ", parray[i].x, parray[i].y);
	}
	log_buf(&out, "(%.2f %.2f)]\n", parray[len - 1].x, parray[len - 1].y);
	dprint(FD_SPRANDOM, "%s", out.buf);
	buf_output_free(&out);
}

/* Comparison function for qsort to sort points by x-value */
static int compare_points(const void *a, const void *b)
{
	/* Cast void pointers to struct point pointers */
	const struct point *point_a = (const struct point *)a;
	const struct point *point_b = (const struct point *)b;

	if (point_a->x < point_b->x) {
		return -1;
	}
	if (point_a->x > point_b->x) {
		return 1;
	}
	return 0;
}

static void reverse(double arr[], size_t size)
{

	size_t left = 0;
	size_t right = size - 1;

	if (size <= 1) {
		return;
	}

	while (left < right) {
		double temp = arr[left];
		arr[left] = arr[right];
		arr[right] = temp;
		left++;
		right--;
	}
}

static double* linspace(double start, double end, unsigned int num)
{
	double *arr;
	unsigned int i;
	double step;

	if (num == 0) {
		return NULL;
	}

	dprint(FD_SPRANDOM, "linespace start=%0.2f end=%0.2f num=%d\n", start, end, num);

	arr = d_alloc(num);
	if (arr == NULL) {
		return NULL;
	}

	if (num == 1) {
		arr[0] = start;
		return arr;
	}

	/* Calculate step size */
	step = (end - start) / ((double)num - 1.0);

	for (i = 0; i < num; i++) {
		arr[i] = start + (double)i * step;
	}

	return arr;
}

static double linear_interp(double new_x, const double *x_arr, const double *y_arr, unsigned int num)
{
	unsigned int i;
	double x1, y1, x2, y2;

	if (num == 0) {
		return 0.0; /* Or handle as an error */
	}

	if (num == 1) {
		return y_arr[0]; /* If only one point, return its y-value */
	}

	/* Handle extrapolation outside the range */
	if (new_x <= x_arr[0]) {
		return y_arr[0];
	}

	if (new_x >= x_arr[num - 1]) {
		return y_arr[num - 1];
	}

	/* Find the interval [x_arr[i], x_arr[i+1]] that contains new_x */
	for (i = 0; i < num - 1; i++) {
		if (new_x >= x_arr[i] && new_x <= x_arr[i+1]) {
			x1 = x_arr[i];
			y1 = y_arr[i];
			x2 = x_arr[i+1];
			y2 = y_arr[i+1];

			/* Avoid division by zero if x values are identical */
			if (fabs(x2 - x1) < 1e-9) { /* Using a small epsilon for float comparison */
				return y1; /* Return y1 if x1 and x2 are almost identical */
			}

			return y1 + (y2 - y1) * ((new_x - x1) / (x2 - x1));
		}
	}
	/* Should not reach here if new_x is within bounds
	 * and x_arr is strictly increasing */
	return 0.0;
}

static int sample_curve_equally_on_x(struct point *points, unsigned int num, unsigned int num_resampled,
                                     struct point **resampled_points)
{
	double *x_orig = (double *)0;
	double *y_orig = (double *)0;
	double *new_x_arr = (double *)0;
	struct point *new_points_arr = (struct point *)0;
	unsigned int i;
	int ret = 0;

	if (points == NULL || resampled_points == NULL) {
		return -3;
	}

	if (num == 0) {
		log_err("Original points array cannot be empty.\n");
		return -3;
	}

	if (num_resampled == 0) {
		*resampled_points = NULL;
		return 0;
	}

	qsort(points, num, sizeof(struct point), compare_points);

	/* Check if x-values are strictly increasing and sort them */
	for (i = 0; i < num - 1; i++) {
		if (points[i+1].x <= points[i].x) {
			log_err("x-values must be strictly increasing.\n");
			ret = -2;
			goto cleanup;
		}
	}

	/* 2. Extract x and y into separate arrays for interpolation */
	/* Use calloc for x_orig and y_orig */
	x_orig = d_alloc(num);
	y_orig = d_alloc(num);
	if (x_orig == NULL || y_orig == NULL) {
		log_err("Memory allocation failed for x_orig or y_orig.\n");
		ret = -1;
		goto cleanup;
	}
	for (i = 0; i < num; i++) {
		x_orig[i] = points[i].x;
		y_orig[i] = points[i].y;
	}


	/* 4. Generate new_x values using linspace */
	new_x_arr = linspace(x_orig[0], x_orig[num - 1], num_resampled);
	if (new_x_arr == NULL) {
		ret = -1; /* linspace_c already printed error */
		goto cleanup;
	}

	/* 5. Allocate memory for new resampled points */
	new_points_arr = (struct point *)calloc(num_resampled, sizeof(struct point));
	if (new_points_arr == (struct point *)0) {
		log_err("Memory allocation failed for new_points_arr.\n");
		ret = -1;
		goto cleanup;
	}

	/* 6. Perform linear interpolation for each new_x to get new_y */
	for (i = 0; i < num_resampled; i++) {
		new_points_arr[i].x = new_x_arr[i];
		new_points_arr[i].y = linear_interp(new_x_arr[i], x_orig, y_orig, num);
	}

	*resampled_points = new_points_arr; /* Assign the result to the output pointer */

cleanup:
	if (x_orig != (double *)0) {
		free(x_orig);
	}
	if (y_orig != (double *)0) {
		free(y_orig);
	}
	if (new_x_arr != (double *)0) {
		free(new_x_arr);
	}

	return ret;
}

/* compute write amplification */
static double compute_waf(double over_provisioning)
{
	return 0.5 / over_provisioning + 0.7;
}

static double compute_validity(double waf)
{
	return 1.0 - (double)1.0 / waf;
}

static double *sprandom_compute(unsigned int n_regions, double over_provisioning)
{
	double waf = compute_waf(over_provisioning);
	double validity = compute_validity(waf);
	double *validity_distribution = NULL;
	double *blocks_ratio = NULL;
	double *acc_ratio = NULL;
	double acc;
	unsigned int i;
	struct point *points = NULL;
	struct point *points_resampled = NULL;
	int ret;

	if (n_regions == 0) {
		log_err("Error: requires at least one region");
		goto out;
	}


	validity_distribution = linspace(1.0, validity, n_regions);

	blocks_ratio = d_alloc(n_regions);
	if (blocks_ratio == NULL) {
		log_err("Memory allocation failed for linspace.\n");
		goto out;
	}

	for (i = 0; i < n_regions; i++) {
		blocks_ratio[i] = 1.0 / validity_distribution[i];
	}

	acc_ratio = d_alloc(n_regions);
	if (acc_ratio == NULL) {
		log_err("Memory allocation failed for linspace_c.\n");
		goto out;
	}

	acc = 0.0;
	for (i = 0; i < n_regions; i++) {
		acc_ratio[i] = acc + blocks_ratio[i];
		acc = acc_ratio[i];
	}

	dprint(FD_SPRANDOM, "validity_distribution:");
	print_d_array(validity_distribution, n_regions);
	dprint(FD_SPRANDOM, "blocks ration:");
	print_d_array(blocks_ratio, n_regions);
	dprint(FD_SPRANDOM, "accumualted ration:");
	print_d_array(acc_ratio, n_regions);

	points = p_alloc(n_regions);

	for (i = 0; i < n_regions; i++) {
		points[i].x = acc_ratio[i];
		points[i].y = validity_distribution[i];
	}
	print_d_points(points, n_regions);

	ret = sample_curve_equally_on_x(points, n_regions, n_regions,
	                                &points_resampled);

	if (ret == 0) {
		printf("Resampled points (num_regions = %u):\n", n_regions);
		print_d_points(points_resampled, n_regions);
	} else {
		log_err("Failed to resample curve. Error code: %d\n", ret);
		goto out;
	}

	for (i = 0; i < n_regions; i++) {
		validity_distribution[i] = points_resampled[i].y;
	}
	dprint(FD_SPRANDOM, "validity resampled:");
	print_d_array(validity_distribution, n_regions);

out:
	free(points);
	free(points_resampled);
	free(blocks_ratio);
	free(acc_ratio);

	reverse(validity_distribution, n_regions);

	return validity_distribution;
}

static uint64_t sprandom_pysical_size(const struct sprandom_info *spr_info,
                                      uint64_t logical_sz)
{
	return logical_sz + ceil((double)logical_sz * spr_info->over_provision);
}


static int sprandom(struct sprandom_info *spr_info, struct fio_file *f,
                    uint64_t align_bs)
{
	uint64_t logical_size = min(f->real_file_size, f->io_size);
	uint64_t physical_size = sprandom_pysical_size(spr_info, logical_size);
	uint64_t region_sz;
	uint64_t region_write_count;
	size_t total_alloc = 0;
	int i;

	double *validity_dist = sprandom_compute(spr_info->num_regions,
	                        spr_info->over_provision);
	if (!validity_dist)
		return -ENOMEM;

	/* Initialize validity_distribution */
	print_d_array(validity_dist, spr_info->num_regions);

	/* FIXME: can we remove it */
	spr_info->validity_dist = validity_dist;
	total_alloc += spr_info->num_regions * sizeof(double); // validity_dist

	/* Precompute invalidity percentage array */
	spr_info->invalid_pct = calloc(spr_info->num_regions, sizeof(int));
	if (!spr_info->invalid_pct) {
		goto err;
	}
	total_alloc += spr_info->num_regions * sizeof(double); // invalid_pct

	for (i = 0; i < spr_info->num_regions; i++) {
		spr_info->invalid_pct[i] = (int)round(((1.0 - validity_dist[i]) * 10000.0));
	}

	region_sz = physical_size / spr_info->num_regions;
	spr_info->region_sz = region_sz;

	dprint(FD_SPRANDOM, "logical_size %ld physical_size %ld region_size %ld num=%d\n",
	       logical_size, physical_size, region_sz, spr_info->num_regions);

	region_write_count = region_sz / align_bs;

	spr_info->invalid_capacity = ceil((double)region_write_count * (1.0 - validity_dist[0]));
	dprint(FD_SPRANDOM, "sprandom inv capacity %ld\n", spr_info->invalid_capacity);

	spr_info->invalid_count[0] = 0;
	spr_info->invalid_count[1] = 0;


	spr_info->invalid_buf = pcb_alloc(spr_info->invalid_capacity + ceil((double)region_write_count * (1.0 - validity_dist[1])));

	total_alloc += 2 * spr_info->invalid_capacity * sizeof(uint64_t);

	spr_info->curr_phase = 0;

	spr_info->current_region = 0;
	spr_info->writes_remaining = region_write_count;
	spr_info->region_write_count = region_write_count;


	/* Display overall allocation */
	{
		char human_buf[32];
		dprint(FD_SPRANDOM, "Overall allocation:\n");
		dprint(FD_SPRANDOM, "  logical_size:      %lu: %s\n",
			logical_size,
			human_mem_to_str(human_buf, sizeof(human_buf), logical_size));
		dprint(FD_SPRANDOM, "  physical_size:     %lu: %s\n",
			physical_size,
			human_mem_to_str(human_buf, sizeof(human_buf), physical_size));
		dprint(FD_SPRANDOM, "  region_size:       %lu\n", region_sz);
		dprint(FD_SPRANDOM, "  num_regions:       %u\n", spr_info->num_regions);
		dprint(FD_SPRANDOM, "  region_write_count:%lu\n", region_write_count);
		dprint(FD_SPRANDOM, "  invalid_capacity:  %lu\n", spr_info->invalid_capacity);
		dprint(FD_SPRANDOM, "  dynamic memory:    %zu: %s\n",
			total_alloc,
			human_mem_to_str(human_buf, sizeof(human_buf), total_alloc));
	}
	return 0;

err:
	free(spr_info->validity_dist);
	free(spr_info->invalid_pct);
	return -ENOMEM;
}

static void sprandom_add_with_probability_2(struct sprandom_info *info,
                uint64_t offset, unsigned int phase)
{
	int v = rand_between(&info->rand_state, 0, 10000);

	if (v <= info->invalid_pct[info->current_region]) {
		if (pcb_space_available(info->invalid_buf)) {
			pcb_push_staged(info->invalid_buf, offset); 
			info->invalid_count[phase]++;
		} else {
			dprint(FD_SPRANDOM, "overriding!!!\n");
		}
	}
}


int sprandom_get_next_offset(struct sprandom_info *info, struct fio_file *f, uint64_t *b)
{
	uint64_t offset = 0;
	uint32_t phase = info->curr_phase;

	/* replay invalidation */
	if (pcb_pop(info->invalid_buf, &offset)) {
		*b = offset;
		sprandom_add_with_probability_2(info, *b,  phase ^ 1);
		dprint(FD_SPRANDOM, "Write %ld over %d\n", *b, info->current_region);
		return 0;
	}

	// Move to next region
	if (info->writes_remaining == 0) {
		if (info->current_region >= info->num_regions) {
			dprint(FD_SPRANDOM, "The End 1 num %d cur%d\n", info->current_region, info->num_regions);
			return 1;
		}

		dprint(FD_SPRANDOM, "Invalidation[%d] %ld %ld %.04f %d\n",
			info->current_region,
			info->region_write_count,
			info->invalid_count[phase],
			(double)info->invalid_count[phase] / (double)info->region_write_count,
			info->invalid_pct[info->current_region]);

		info->writes_remaining = info->region_write_count - info->invalid_count[phase];
		info->invalid_count[phase] = 0;

		info->current_region++;
		info->curr_phase  = phase ^ 1;
		pcb_commit(info->invalid_buf);
		dprint(FD_SPRANDOM, "region count %d\n", info->current_region);
	}

	if (lfsr_next(&f->lfsr, &offset)) {
		dprint(FD_SPRANDOM, "The End: LFSR exhausted %d [%ld] [%ld]\n",
			info->current_region,
			info->invalid_count[phase],
			info->invalid_count[phase ^ 1]);

		dprint(FD_SPRANDOM, "Invalidation[%d] %ld %ld %.04f %d\n",
			info->current_region,
			info->region_write_count,
			info->invalid_count[phase],
			(double)info->invalid_count[phase] / (double)info->region_write_count,
			info->invalid_pct[info->current_region]);
			return 1;
	}

	if (info->writes_remaining > 0)
		info->writes_remaining--;

	sprandom_add_with_probability_2(info, offset,  phase ^ 1);
	dprint(FD_SPRANDOM, "Write %ld lfsr %d\n", offset, info->current_region);
	*b = offset;
	return 0;
}

int sprandom_init(struct thread_data *td, struct fio_file *f)
{
	if (!td->o.sprandom)
		return 0;

	dprint(FD_SPRANDOM, "initializtion bs=%lld\n", td->o.rw_min_bs);
	if (td->o.rw_min_bs != td->o.bs[DDIR_WRITE]) {
		dprint(FD_SPRANDOM, "initializtion bs=%lld != bs=%lld\n", td->o.bs[DDIR_WRITE], td->o.rw_min_bs);
	}

	f->spr_info = calloc(sizeof(*f->spr_info), 1);
	if (!f->spr_info)
		return -ENOMEM;

	f->spr_info->num_regions = td->o.num_regions;
	f->spr_info->over_provision = td->o.over_provisioning.u.f;
	td->o.io_size = sprandom_pysical_size(f->spr_info, min(f->real_file_size, f->io_size)) * 2;
	init_rand_seed(&f->spr_info->rand_state, 123234, 0);

	return sprandom(f->spr_info, f, td->o.bs[DDIR_WRITE]);
}

void sprandom_free(struct sprandom_info *spr_info)
{
	if (!spr_info)
		return;

	if (spr_info->validity_dist)
		free(spr_info->validity_dist);

	if (spr_info->invalid_buf)
		free(spr_info->invalid_buf);
	free(spr_info);
}
