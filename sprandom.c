/**
 * SPDX-License-Identifier: GPL-2.0 only
 *
 * Copyright (c) 2025 Sandisk Corporation or its affiliates.
 */
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "flist.h"
#include "sprandom.h"
#include "pshared.h"
#ifndef SPRANDOM_UNIT_TEST
#include "fio.h"
#include "file.h"
#else
#include <assert.h>
#include <stdio.h>
#endif /* SPRANDOM_UNIT_TESTS */

static inline double *d_alloc(size_t n)
{
    return malloc(n * sizeof(double));
}

struct point {
    double x;
    double y;
};

static inline struct point *p_alloc(size_t n)
{
    return malloc(n * sizeof(struct point));
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
        fprintf(stderr, "Error: Original points array cannot be empty.\n");
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
            fprintf(stderr, "Error: x-values must be strictly increasing.\n");
            ret = -2;
            goto cleanup;
        }
    }

    /* 2. Extract x and y into separate arrays for interpolation */
    /* Use calloc for x_orig and y_orig */
    x_orig = d_alloc(num);
    y_orig = d_alloc(num);
    if (x_orig == NULL || y_orig == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for x_orig or y_orig.\n");
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
        fprintf(stderr, "Error: Memory allocation failed for new_points_arr.\n");
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

static double compute_write_amplification(double over_provisioning)
{
    return 0.5 / over_provisioning + 0.7;
}

static double compute_validity(double write_amplification)
{
    return 1.0 - (double)1.0 / write_amplification;
}

static double *sprandom_compute(unsigned int n_regions, double over_provisioning)
{
    double write_amplification = compute_write_amplification(over_provisioning);
    double validity = compute_validity(write_amplification);
    double *validity_distribution = NULL;
    double *blocks_ratio = NULL;
    double *acc_ratio = NULL;
    double acc;
    unsigned int i;
    struct point *points = NULL;
    struct point *points_resampled = NULL;
    int ret;

    validity_distribution = linspace(1.0, validity, n_regions);

    blocks_ratio = d_alloc(n_regions);
    if (blocks_ratio == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for linspace.\n");
        goto out;
    }

    for (i = 0; i < n_regions; i++) {
        blocks_ratio[i] = 1.0 / validity_distribution[i];
    }

    acc_ratio = d_alloc(n_regions);
    if (acc_ratio == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for linspace_c.\n");
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

    ret = sample_curve_equally_on_x(points, n_regions, n_regions, &points_resampled);

    if (ret == 0) {
        printf("Resampled points (num_regions = %u):\n", n_regions);
        print_d_points(points_resampled, n_regions);
    } else {
        fprintf(stderr, "Failed to resample curve. Error code: %d\n", ret);
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

    return validity_distribution;
}

static int sprandom(struct sprandom_info *spr_info, struct fio_file *f,
                    uint64_t align_bs)
{
    uint64_t logical_size =  f->real_file_size;
    double over_provision = spr_info->over_provision;
    uint64_t physical_size = logical_size + ceil((double)logical_size * over_provision);
    uint64_t region_sz = physical_size / spr_info->nregions;
    uint64_t increment;
    unsigned int i;

    double *validity_distribution = sprandom_compute(spr_info->nregions,
                                                      spr_info->over_provision);

    if (!validity_distribution) {
        return -ENOMEM;
    }

    region_sz = (region_sz / align_bs + (region_sz % align_bs != 0)) * align_bs;

    spr_info->region_sz = region_sz;

    dprint(FD_FILE, "sprandom region size %ld\n", region_sz);

    reverse(validity_distribution, spr_info->nregions);
    print_d_array(validity_distribution, spr_info->nregions);
    spr_info->offsets[0] = 0;
    for (i = 1; i < spr_info->nregions; i++) {
        increment = (uint64_t)ceil(validity_distribution[i] * region_sz);
        spr_info->offsets[i] = spr_info->offsets[i-1] + increment;
    }
    free(validity_distribution);
    print_ld_array(spr_info->offsets, spr_info->nregions);

     return 0;
}

static int sprandom_create_info(struct thread_data *td, struct fio_file *f)

{
    struct sprandom_info *spr_info = NULL;
    uint32_t num_regions = td->o.num_regions;

    spr_info = scalloc(1, sizeof(*spr_info) +
                       num_regions * sizeof(spr_info->offsets[0]));
    if (!spr_info) {
        return -ENOMEM;
    }

    mutex_init_pshared(&spr_info->mutex);
    spr_info->refcount = 1;
    spr_info->nregions = num_regions;
    spr_info->over_provision = td->o.over_provisioning.u.f;
    sprandom(spr_info, f, td_min_bs(td));
    f->spr_info = spr_info;

    return 0;
}

void sprandom_free_info(struct fio_file *f)
{
    uint32_t refcount;

    dprint(FD_FILE, "sprandom free info for %s\n", f->file_name);

    pthread_mutex_lock(&f->spr_info->mutex);
    refcount = --f->spr_info->refcount;
    pthread_mutex_unlock(&f->spr_info->mutex);

    assert((int32_t)refcount >= 0);
    if (refcount == 0) {
        sfree(f->spr_info);
    }
    f->spr_info = NULL;
}

static int sprandom_init_info(struct thread_data *td, struct fio_file *file)
{
    struct fio_file *f2;
    int j, ret;

    dprint(FD_FILE, "sprandom init info for %s\n", file->file_name);

    for_each_td(td2) {
        for_each_file(td2, f2, j) {
            if (td2 == td && f2 == file)
                continue;
            if (!f2->spr_info ||
                strcmp(f2->file_name, file->file_name) != 0)
                continue;
            file->spr_info = f2->spr_info;
            file->spr_info->refcount++;
            return 0;
        }
    } end_for_each();

    ret = sprandom_create_info(td, file);
    if (ret < 0) {
        td_verror(td, -ret, "sprandom_create_info() failed");
    }
    dprint(FD_FILE, "sprandom created for [%d] %s\n", td->subjob_number, file->file_name);

    return ret;
}

int sprandom_init_files(struct thread_data *td)
{
    struct fio_file *f;
    int i;
    for_each_file(td, f, i) {
        if (sprandom_init_info(td, f))
            return 1;
    }
    return 0;
}

uint64_t sprandom_io_size(const struct fio_file *f)
{
    if (f->spr_info)
        return f->spr_info->region_sz;
    return 0;
}
