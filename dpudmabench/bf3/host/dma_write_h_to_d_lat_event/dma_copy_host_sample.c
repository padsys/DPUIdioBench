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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_DPU);

#define SLEEP_IN_NANOS (1 * 1000)	/* Sample the job every 10 microseconds  */
#define MAX_DMA_BUF_SIZE (1024 * 1024)	/* DMA buffer maximum size */
#define RECV_BUF_SIZE 256		/* Buffer which contains config information */

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
save_config_info_to_buffers(const char *export_desc_file_path, const char *buffer_info_file_path, char *export_desc,
			    size_t *export_desc_len, char **remote_addr, size_t *remote_addr_len)
{
	FILE *fp;
	long file_size;
	char buffer[RECV_BUF_SIZE];

	fp = fopen(export_desc_file_path, "r");
	if (fp == NULL) {
		DOCA_LOG_ERR("Failed to open %s", export_desc_file_path);
		return DOCA_ERROR_IO_FAILED;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		DOCA_LOG_ERR("Failed to calculate file size");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}

	file_size = ftell(fp);
	if (file_size == -1) {
		DOCA_LOG_ERR("Failed to calculate file size");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}

	if (file_size > MAX_DMA_BUF_SIZE)
		file_size = MAX_DMA_BUF_SIZE;

	*export_desc_len = file_size;

	if (fseek(fp, 0L, SEEK_SET) != 0) {
		DOCA_LOG_ERR("Failed to calculate file size");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}

	if (fread(export_desc, 1, file_size, fp) != file_size) {
		DOCA_LOG_ERR("Failed to allocate memory for source buffer");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}

	fclose(fp);

	/* Read source buffer information from file */
	fp = fopen(buffer_info_file_path, "r");
	if (fp == NULL) {
		DOCA_LOG_ERR("Failed to open %s", buffer_info_file_path);
		return DOCA_ERROR_IO_FAILED;
	}

	/* Get source buffer address */
	if (fgets(buffer, RECV_BUF_SIZE, fp) == NULL) {
		DOCA_LOG_ERR("Failed to read the source (host) buffer address");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}
	*remote_addr = (char *)strtoull(buffer, NULL, 0);

	memset(buffer, 0, RECV_BUF_SIZE);

	/* Get source buffer length */
	if (fgets(buffer, RECV_BUF_SIZE, fp) == NULL) {
		DOCA_LOG_ERR("Failed to read the source (host) buffer length");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}
	*remote_addr_len = strtoull(buffer, NULL, 0);

	fclose(fp);

	return DOCA_SUCCESS;
}

/*
 * Run DOCA DMA DPU copy sample
 *
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer info file path
 * @pcie_addr [in]: Device PCI address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
dma_copy_dpu(char *export_desc_file_path, char *buffer_info_file_path, const char *pcie_addr)
{
	struct dma_resources resources;
	struct program_core_objects *state = &resources.state;
	struct doca_dma_task_memcpy *dma_task = NULL;
	struct doca_task *task = NULL;
	union doca_data task_user_data = {0};
	// for (int i = 0; i < N; i++) {
	// 	resources.dst_doca_buf_array[i] = NULL;
	// }
	char export_desc[1024] = {0};
	size_t export_desc_len = 0;
	size_t max_buffer_size;
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};
	doca_error_t result, tmp_result, task_result;
	struct timespec start, blk_end, non_blk_end;
	unsigned long long total_time_in_ns = 0;
	double blk_ns = 0; //non_blk_ns = 0;
	int i;

	/* Allocate resources */
	result = allocate_dma_resources_with_event(pcie_addr, &resources);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate DMA resources: %s", doca_error_get_descr(result));
		return result;
	}

	/* Connect context to progress engine */
	result = doca_pe_connect_ctx(state->pe, state->ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to connect progress engine to context: %s", doca_error_get_descr(result));
		goto destroy_resources;
	}

	result = doca_ctx_start(state->ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start context: %s", doca_error_get_descr(result));
		goto destroy_resources;
	}

	/* Get maximum buffer size allowed */
	result = doca_dma_cap_task_memcpy_get_max_buf_size(doca_dev_as_devinfo(state->dev), &max_buffer_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get max buffer size: %s", doca_error_get_descr(result));
		goto stop_dma;
	}
	printf("Max buffer size: %zu\n", max_buffer_size);

	/* Copy all relevant information into local buffers */
	result = save_config_info_to_buffers(export_desc_file_path, buffer_info_file_path, export_desc, &export_desc_len,
				    &(resources.remote_addr), &(resources.remote_addr_len));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to read memory configuration from file: %s", doca_error_get_descr(result));
		goto stop_dma;
	}

	/* Copy the entire host buffer */
	resources.dst_buffer_size = resources.remote_addr_len;
	resources.dpu_buffer = (char *)malloc(resources.dst_buffer_size);
	memset(resources.dpu_buffer, '0', resources.dst_buffer_size);
	// resources.dpu_buffer[resources.dst_buffer_size - 1] = '\0';
	if (resources.dpu_buffer == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for destination buffer");
		goto stop_dma;
	}
	// print dpu_buffer content
	// printf("dpu_buffer (remote_addr_len is %d): %s\n", remote_addr_len, dpu_buffer);
	printf("Iteration %d, dst_buffer_size: %d\n", N, resources.dst_buffer_size);

	result = doca_mmap_set_memrange(state->dst_mmap, resources.dpu_buffer, resources.dst_buffer_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set memory range for destination mmap: %s", doca_error_get_descr(result));
		goto free_dpu_buffer;
	}

	result = doca_mmap_start(state->dst_mmap);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start destination mmap: %s", doca_error_get_descr(result));
		goto free_dpu_buffer;
	}
	DOCA_LOG_INFO("Destination mmap started");
	/* Create a local DOCA mmap from exported data */
	result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, state->dev,
					      &(resources.remote_mmap));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create mmap from export: %s", doca_error_get_descr(result));
		goto free_dpu_buffer;
	}
	DOCA_LOG_INFO("Remote mmap created");

	/* Construct DOCA buffer for each address range */
// read
	result = doca_buf_inventory_buf_get_by_addr(state->buf_inv, resources.remote_mmap, resources.remote_addr, resources.remote_addr_len, &(resources.src_doca_buf));
// write
	// result = doca_buf_inventory_buf_get_by_addr(state->buf_inv, resources.remote_mmap, resources.remote_addr, resources.remote_addr_len, &(resources.dst_doca_buf));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s",
			     doca_error_get_descr(result));
		goto destroy_remote_mmap;
	}
	DOCA_LOG_INFO("Remote buffer acquired");


	/* Construct DOCA buffer for each address range */
// read
	result = doca_buf_inventory_buf_get_by_addr(state->buf_inv, state->dst_mmap, resources.dpu_buffer, resources.dst_buffer_size, &(resources.dst_doca_buf));
// write
	// result = doca_buf_inventory_buf_get_by_addr(state->buf_inv, state->dst_mmap, resources.dpu_buffer, resources.dst_buffer_size, &(resources.src_doca_buf));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s",
			     doca_error_get_descr(result));
		goto destroy_src_buf;
	}

	DOCA_LOG_INFO("Destination buffer acquired");

	/* Set data position in src_buff */
// read
	result = doca_buf_set_data(resources.src_doca_buf, resources.remote_addr, resources.dst_buffer_size);
// write
	// result = doca_buf_set_data(resources.src_doca_buf, resources.dpu_buffer, resources.dst_buffer_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set data for DOCA source buffer: %s", doca_error_get_descr(result));

		goto destroy_dst_buf;
	}
	DOCA_LOG_INFO("Data set for source buffer");

	/* Include result in user data of task to be used in the callbacks */
	task_result = -1;
	task_user_data.ptr = &task_result;
	resources.num_remaining_tasks = N;

	/* Allocate and construct DMA task */
	result = doca_dma_task_memcpy_alloc_init(resources.dma_ctx, resources.src_doca_buf, resources.dst_doca_buf, task_user_data, &dma_task);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate DMA memcpy task: %s", doca_error_get_descr(result));
		goto destroy_dst_buf;
	}

	task = doca_dma_task_memcpy_as_task(dma_task);
	resources.run_main_loop = true;

	// struct timespec test_start, test_end;
	// clock_gettime(CLOCK_REALTIME, &test_start);
	// for (int k = 0; k < N; k++) {
	// 	struct doca_buf *dst = doca_dma_task_memcpy_get_dst(dma_task);
	// 	doca_error_t status = DOCA_SUCCESS;
	// 	status = doca_buf_reset_data_len(resources.dst_doca_buf);
	// 	if (status != DOCA_SUCCESS) {
	// 		DOCA_LOG_ERR("Failed to reset data length for destination buffer: %s", doca_error_get_descr(status));
	// 	}
	// 	status = doca_buf_dec_refcount(dst, NULL);
	// 	if (status != DOCA_SUCCESS) {
	// 		DOCA_LOG_ERR("Failed to decrement reference count for destination buffer: %s", doca_error_get_descr(status));
	// 	}
	// 	status = doca_buf_inventory_buf_get_by_addr(state->buf_inv, state->dst_mmap, resources.dpu_buffer, resources.dst_buffer_size, &(resources.dst_doca_buf));
	// 	doca_dma_task_memcpy_set_dst(dma_task, resources.dst_doca_buf);
	// 	struct doca_task *task = doca_dma_task_memcpy_as_task(dma_task);
	// 	doca_task_free(doca_dma_task_memcpy_as_task(dma_task));
	// }
	// clock_gettime(CLOCK_REALTIME, &test_end);
	// double test_ns = (test_end.tv_sec - test_start.tv_sec)*1000000000 + (test_end.tv_nsec - test_start.tv_nsec);
	// double mean_ttest = (double)test_ns/N;
	// printf("Time to get dst_doca_buf: %13.2f\n", mean_ttest/1000);


	clock_gettime(CLOCK_REALTIME, &start);

	/* Submit DMA task */
	// clock_gettime(CLOCK_REALTIME, &(resources.blk_time_start[0]));
	result = doca_task_submit(task);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit DMA task: %s", doca_error_get_descr(result));
		goto destroy_dst_buf;
	}

// /* poll for complete */
// 	while (resources.num_remaining_tasks != 0) {
// 		(void)doca_pe_progress(state->pe);
// 	}
// /* end */

/* run for completion with event */
	static const int no_timeout = -1;
	struct epoll_event ep_event = {0};
	int epoll_status = 0;
	// int count = 0;

	do {
		/**
		 * The internal loop shall run as long as progress one returns 1 because it implies that there may be
		 * more tasks to complete. Once it returns 0 the external loop shall arm the PE event (by calling
		 * doca_pe_request_notification) and shall wait until the event is fired, signaling that there is a
		 * completed task to progress.
		 * Calling doca_pe_request_notification implies enabling an interrupt, but it also reduces CPU
		 * utilization because the program can sleep until the event is fired.
		 */
		while (doca_pe_progress(state->pe) != 0) {
			// if (state->base.num_completed_tasks == NUM_BUFFER_PAIRS) {
			if (resources.num_remaining_tasks == 0) {
				DOCA_LOG_INFO("All tasks completed");
				goto END_TIME;
			}
		}

		/**
		 * Calling doca_pe_request_notification arms the PE event. The event will be signaled when a task is
		 * completed.
		 */
		result = doca_pe_request_notification(state->pe);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to request notification: %s", doca_error_get_descr(result));
			return result;
		}

		epoll_status = epoll_wait(state->epoll_fd, &ep_event, 5, no_timeout);
		// count++;
		if (epoll_status == -1) {
			DOCA_LOG_ERR("Failed waiting for event, error=%d", errno);
			return DOCA_ERROR_OPERATING_SYSTEM;
		}

		/* handle parameter is not used in Linux */
		result = doca_pe_clear_notification(state->pe, 0);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to clear notification: %s", doca_error_get_descr(result));
			return result;
		}
	} while (1);

/* end */
END_TIME:
	clock_gettime(CLOCK_REALTIME, &blk_end);

	// dpu_buffer[dst_buffer_size * N - 1] = '\0';
	// DOCA_LOG_INFO("Memory content: %s", dpu_buffer);
	DOCA_LOG_INFO("******************* DMA task done");

	result = task_result;

	blk_ns = (blk_end.tv_sec - start.tv_sec)*1000000000 + (blk_end.tv_nsec - start.tv_nsec);
	total_time_in_ns = blk_ns;
	double mean_t = (double)total_time_in_ns/N;
	printf("Blocking DMA read\n");
	printf("Avg Lat(us)\n");
	printf("%13.2f\n", mean_t/1000);
	DOCA_LOG_INFO("Host sample can be closed, DMA copy ended");

	// double blk_time[N];
	// total_time_in_ns = 0;
	// for (i = 0; i < N; i++) {
	// 	blk_time[i] = (resources.blk_time_end[i].tv_sec - resources.blk_time_start[i].tv_sec)*1000000000 + (resources.blk_time_end[i].tv_nsec - resources.blk_time_start[i].tv_nsec);
	// }

	// double min_t = blk_time[0];
	// double max_t = blk_time[0];
	// for (i = 0; i < N; i++) {
	// 		if (blk_time[i] <= min_t)
	// 				min_t = blk_time[i];
	// 		if (blk_time[i] >= max_t)
	// 				max_t = blk_time[i];
	// 		total_time_in_ns += blk_time[i];
	// }
	// double mean_t_new = (double)total_time_in_ns/N;
	// double std_dev = 0;
	// for (i = 0; i < N; i++) {
	// 		std_dev += pow(abs(blk_time[i]-mean_t_new),2);
	// }
	// std_dev = sqrt(std_dev/N);
	// printf("Blocking DMA read\n");
	// printf("Min time(us)\t Avg Lat(us)\t Max time(us)\t Std dev(us)\n");
	// printf("%.2f\t %13.2f\t %13.2f\t %13.2f\n", min_t/1000, mean_t_new/1000, max_t/1000, std_dev/1000);

destroy_dst_buf:
	tmp_result = doca_buf_dec_refcount(resources.dst_doca_buf, NULL);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_ERROR_PROPAGATE(result, tmp_result);
		DOCA_LOG_ERR("Failed to decrease DOCA destination buffer reference count: %s", doca_error_get_descr(tmp_result));
	}
destroy_src_buf:
	tmp_result = doca_buf_dec_refcount(resources.src_doca_buf, NULL);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_ERROR_PROPAGATE(result, tmp_result);
		DOCA_LOG_ERR("Failed to decrease DOCA source buffer reference count: %s", doca_error_get_descr(tmp_result));
	}
destroy_remote_mmap:
	tmp_result = doca_mmap_destroy(resources.remote_mmap);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_ERROR_PROPAGATE(result, tmp_result);
		DOCA_LOG_ERR("Failed to decrease remote mmap: %s", doca_error_get_descr(tmp_result));
	}
free_dpu_buffer:
	free(resources.dpu_buffer);
stop_dma:
	tmp_result = doca_ctx_stop(state->ctx);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_ERROR_PROPAGATE(result, tmp_result);
		DOCA_LOG_ERR("Unable to stop context: %s", doca_error_get_descr(tmp_result));
	}
	state->ctx = NULL;
destroy_resources:
	tmp_result = destroy_dma_resources(&resources);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_ERROR_PROPAGATE(result, tmp_result);
		DOCA_LOG_ERR("Failed to destroy DMA resources: %s", doca_error_get_descr(tmp_result));
	}

	return result;
}