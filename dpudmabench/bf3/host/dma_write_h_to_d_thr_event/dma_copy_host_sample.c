/*
* Copyright (c) 2025, University of California, Merced. All rights reserved.
*
* This file is part of the benchmarking software package developed by
* the team members of Prof. Xiaoyi Lu's group at University of California, Merced.
*
* For detailed copyright and licensing information, please refer to the license
* file LICENSE in the top level directory.
*
*/
/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_DPU);

#define EVENT_REPS 100       /* batches in event test */
#define BATCH_SIZE N         /* = NUM_DMA_TASKS = 1024 */
#define ITERATIONS 1000

/*
 * Saves export descriptor and buffer information content into memory buffers
 *
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer information file path
 * @export_desc [in]: Export descriptor buffer
 * @export_desc_len [in]: Export descriptor buffer length
 * @remote_addr [in]: Remote buffer address
 * @remote_addr_len [in]: Remote buffer total length
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t 
save_config_info_to_buffers(const char *export_desc_file_path,
                                                const char *buffer_info_file_path,
                                                void **export_desc,
                                                size_t *export_desc_len,
                                                char **remote_addr,
                                                size_t *remote_addr_len) {
  FILE *fp;
  long fsize;

  // 1) Read the export descriptor blob
  fp = fopen(export_desc_file_path, "rb"); // ← binary mode!
  if (!fp)
    return DOCA_ERROR_IO_FAILED;
  if (fseek(fp, 0, SEEK_END) != 0 || (fsize = ftell(fp)) < 0 ||
      fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return DOCA_ERROR_IO_FAILED;
  }
  *export_desc_len = (size_t)fsize;
  *export_desc = malloc(*export_desc_len);
  if (!*export_desc) {
    fclose(fp);
    return DOCA_ERROR_NO_MEMORY;
  }
  if (fread(*export_desc, 1, *export_desc_len, fp) != *export_desc_len) {
    fclose(fp);
    free(*export_desc);
    return DOCA_ERROR_IO_FAILED;
  }
  fclose(fp);

  // 2) Read the remote‐address + length file
  char buf[256];
  fp = fopen(buffer_info_file_path, "r");
  if (!fp) {
    free(*export_desc);
    return DOCA_ERROR_IO_FAILED;
  }
  if (!fgets(buf, sizeof(buf), fp)) {
    fclose(fp);
    free(*export_desc);
    return DOCA_ERROR_IO_FAILED;
  }
  *remote_addr = (char *)strtoull(buf, NULL, 0);
  if (!fgets(buf, sizeof(buf), fp)) {
    fclose(fp);
    free(*export_desc);
    return DOCA_ERROR_IO_FAILED;
  }
  *remote_addr_len = strtoull(buf, NULL, 0);
  fclose(fp);

  return DOCA_SUCCESS;
}

static void thr_completed_cb(struct doca_dma_task_memcpy *dma_task,
                             union doca_data task_user_data,
                             union doca_data ctx_user_data) {
  struct dma_resources *resources = (struct dma_resources *)ctx_user_data.ptr;

  resources->num_remaining_tasks--;
  if (resources->num_remaining_tasks == 0) {
    // printf("***** all tasks completed\n");
    // fflush(stdout);
  }
}

static void thr_error_cb(struct doca_dma_task_memcpy *cpy,
                         union doca_data task_ud, union doca_data ctx_ud) {
  (void)cpy;
  (void)task_ud;
  unsigned long long *rem = (unsigned long long *)ctx_ud.ptr;
  (*rem)--;
  DOCA_LOG_ERR("DMA task failed");
}

/*
 * Run DOCA DMA DPU copy sample
 *
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer info file path
 * @pcie_addr [in]: Device PCI address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t dma_copy_host(char *export_desc_file_path, char *buffer_info_file_path,
                          const char *pci_addr) {
  struct dma_resources res;
  struct program_core_objects *st = &res.state;
  doca_error_t result;

  void *export_desc_blob = NULL;
  size_t export_desc_len = 0;
  char *remote_addr = NULL;
  size_t remote_addr_len = 0;
  size_t dst_size;
  char *dpu_buf;

  result = allocate_dma_resources_with_event(pci_addr, &res);
  if (result != DOCA_SUCCESS)
    return result;
  doca_pe_connect_ctx(st->pe, st->ctx);

  result = doca_dma_task_memcpy_set_conf(res.dma_ctx, thr_completed_cb,
                                         thr_error_cb, BATCH_SIZE);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to set DMA callbacks: %s",
                 doca_error_get_descr(result));
    return result;
  } else {
    DOCA_LOG_INFO("DMA callbacks set successfully");
  }

  doca_ctx_start(st->ctx);

  result = doca_dma_cap_task_memcpy_get_max_buf_size(
      doca_dev_as_devinfo(st->dev), &dst_size);
  if (result != DOCA_SUCCESS)
    return result;

  result = save_config_info_to_buffers(export_desc_file_path, buffer_info_file_path,
                                       &export_desc_blob, &export_desc_len,
                                       &remote_addr, &remote_addr_len);
  if (result != DOCA_SUCCESS)
    return result;

  dpu_buf = malloc(dst_size);
  if (!dpu_buf)
    return DOCA_ERROR_NO_MEMORY;
  memset(dpu_buf, 0, dst_size);
  doca_mmap_set_memrange(st->dst_mmap, dpu_buf, dst_size);
  doca_mmap_start(st->dst_mmap);

  result = doca_mmap_create_from_export(NULL, export_desc_blob, export_desc_len,
                                        st->dev, &res.remote_mmap);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create remote mmap: %s",
                 doca_error_get_descr(result));
    return result;
  } else {
    DOCA_LOG_INFO("Remote mmap created successfully");
  }

  for (int i = 0; i < BATCH_SIZE; i++) {
    result = doca_buf_inventory_buf_get_by_addr(st->buf_inv, res.remote_mmap,
                                                remote_addr, remote_addr_len,
                                                &res.src_doca_buf_array[i]);
    if (result != DOCA_SUCCESS)
      return result;

    result = doca_buf_inventory_buf_get_by_addr(st->buf_inv, st->dst_mmap,
                                                dpu_buf, dst_size,
                                                &res.dst_doca_buf_array[i]);
    if (result != DOCA_SUCCESS)
      return result;

    result = doca_buf_set_data(res.src_doca_buf_array[i], remote_addr,
                               remote_addr_len);
    if (result != DOCA_SUCCESS)
      return result;
  }

  union doca_data cb_data = {.ptr = &res.num_remaining_tasks};

  for (int i = 0; i < BATCH_SIZE; i++) {
    struct doca_dma_task_memcpy *t;
    result =
        doca_dma_task_memcpy_alloc_init(res.dma_ctx, res.src_doca_buf_array[i],
                                        res.dst_doca_buf_array[i], cb_data, &t);
    if (result != DOCA_SUCCESS)
      return result;
    res.tasks[i] = t;
  }
  printf("Event-driven throughput test starting!\n");

  struct timespec ts = {0};

  double times[ITERATIONS];
  unsigned long long total_time_ns = 0;
  struct timespec start, end;
  double min_t, max_t, mean_t, stddev_t;
  struct epoll_event events[5];

  for (int i = 0; i < ITERATIONS; i++) {
    res.num_remaining_tasks = BATCH_SIZE;

    doca_pe_request_notification(st->pe);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int j = 0; j < BATCH_SIZE; j++) {
      result = doca_task_submit(doca_dma_task_memcpy_as_task(res.tasks[j]));
      if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Submit error: %s", doca_error_get_descr(result));
        return result;
      }
    }

    int nevents = epoll_wait(st->epoll_fd, events, 5, -1);
    if (nevents < 0) {
      perror("epoll_wait failed");
      return DOCA_ERROR_IO_FAILED;
    }

    doca_pe_clear_notification(st->pe, 0);
    while (res.num_remaining_tasks > 0 && doca_pe_progress(st->pe) > 0)
      ;

    clock_gettime(CLOCK_MONOTONIC, &end);
    double delta_ns =
        (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    times[i] = delta_ns;
    total_time_ns += delta_ns;
  }

  min_t = max_t = times[0];
  for (int i = 1; i < ITERATIONS; i++) {
    if (times[i] < min_t)
      min_t = times[i];
    if (times[i] > max_t)
      max_t = times[i];
  }
  mean_t = total_time_ns / (double)ITERATIONS;

  stddev_t = 0.0;
  for (int i = 0; i < ITERATIONS; i++)
    stddev_t += pow(times[i] - mean_t, 2);
  stddev_t = sqrt(stddev_t / ITERATIONS);

  printf("Event-driven DMA throughput (per %d-copy batch):\n", BATCH_SIZE);
  printf("Min time(us)\tAvg Lat(us)\tMax time(us)\tStd dev(us)\n");
  printf("%.2f\t%.2f\t%.2f\t%.2f\n", min_t / 1000, mean_t / 1000, max_t / 1000,
         stddev_t / 1000);
  unsigned long long total_ops = BATCH_SIZE * EVENT_REPS;
  double elapsed_ns = total_time_ns;
  double kops = (double)total_ops / elapsed_ns * 1e6;

  printf("Event-driven DMA Throughput: %.2f Kops/s\n", kops);

  for (int i = 0; i < BATCH_SIZE; i++)
    doca_task_free(doca_dma_task_memcpy_as_task(res.tasks[i]));

  for (int i = 0; i < BATCH_SIZE; i++) {
    doca_buf_dec_refcount(res.src_doca_buf_array[i], NULL);
    doca_buf_dec_refcount(res.dst_doca_buf_array[i], NULL);
  }
  doca_mmap_destroy(res.remote_mmap);
  free(dpu_buf);
  doca_ctx_stop(st->ctx);
  destroy_dma_resources(&res);

  return DOCA_SUCCESS;
}