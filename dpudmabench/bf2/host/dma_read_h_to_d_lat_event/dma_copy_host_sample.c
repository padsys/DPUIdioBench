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
#include <sys/epoll.h>
#include <unistd.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_HOST);

#define SLEEP_IN_NANOS (1)	/* Sample the job every 1 nanoseconds  */
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
dma_copy_from_dpu(char *export_desc_file_path, char *buffer_info_file_path, struct doca_pci_bdf *pcie_addr)
{
	struct program_core_objects state = {0};
	struct doca_event event = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	struct doca_dma *dma_ctx;
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	struct doca_mmap *remote_mmap;
	doca_error_t result;
	struct timespec ts = {0};
	uint32_t max_chunks = 2;
	char export_desc[1024] = {0};
	char *remote_addr = NULL;
	char *dpu_buffer;
	size_t dst_buffer_size, remote_addr_len = 0, export_desc_len = 0;

	result = doca_dma_create(&dma_ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create DMA engine: %s", doca_get_error_string(result));
		return result;
	}

	state.ctx = doca_dma_as_ctx(dma_ctx);

	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state.dev);
	if (result != DOCA_SUCCESS) {
		doca_dma_destroy(dma_ctx);
		return result;
	}

	result = init_core_objects(&state, DOCA_BUF_EXTENSION_NONE, WORKQ_DEPTH, max_chunks);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&state, dma_ctx);
		return result;
	}

        doca_event_handle_t handle;
        result = doca_workq_get_event_handle(state.workq, &handle);
        if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Unable to get event handle: %s", doca_get_error_string(result));
                return result;
        }

        result = doca_workq_event_handle_arm(state.workq);
        if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Unable to arm WorkQ: %s", doca_get_error_string(result));
                return result;
        }

        // Initialize epoll()
        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
                fprintf(stderr, "Failed to create epoll file descriptor\n");
                return 1;
        }
        struct epoll_event e_event, events[5];
        e_event.events = EPOLLIN;
        e_event.data.fd = handle;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, handle, &e_event)) {
                fprintf(stderr, "Failed to add file descriptor to epoll\n");
                close(handle);
                return 1;
        }

	/* Copy all relevant information into local buffers */
	save_config_info_to_buffers(export_desc_file_path, buffer_info_file_path, export_desc, &export_desc_len,
				    &remote_addr, &remote_addr_len);

	/* Copy the entire host buffer */
	dst_buffer_size = remote_addr_len;
	//printf("dst_buffer_size is %ld\n", dst_buffer_size);
	dpu_buffer = (char *)malloc(dst_buffer_size);
	if (dpu_buffer == NULL) {
		DOCA_LOG_ERR("Failed to allocate buffer memory");
		dma_cleanup(&state, dma_ctx);
		return DOCA_ERROR_NO_MEMORY;
	}

	result = doca_mmap_populate(state.mmap, dpu_buffer, dst_buffer_size, PAGE_SIZE, NULL, NULL);
	if (result != DOCA_SUCCESS) {
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Create a local DOCA mmap from exported data */
	result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, state.dev,
					   &remote_mmap);
	if (result != DOCA_SUCCESS) {
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state.buf_inv, remote_mmap, remote_addr, remote_addr_len, &src_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s",
			     doca_get_error_string(result));
		doca_mmap_destroy(remote_mmap);
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, dpu_buffer, dst_buffer_size, &dst_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s",
			     doca_get_error_string(result));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		doca_mmap_destroy(remote_mmap);
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DMA job */
	dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
	dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
	dma_job.base.ctx = state.ctx;
	dma_job.dst_buff = dst_doca_buf;
	dma_job.src_buff = src_doca_buf;

	/* Set data position in src_buff */
	result = doca_buf_set_data(src_doca_buf, remote_addr, dst_buffer_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set data for DOCA buffer: %s", doca_get_error_string(result));
		return result;
	}

        uint8_t p = 1;
        result = doca_ctx_get_event_driven_supported(state.ctx, &p);
        if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Does not support event driven mode: %s", doca_get_error_string(result));
                return result;
        }

	struct timespec start, end;
	unsigned long long total_time_in_ns = 0;
	double ns = 0;
	int N = 5000;
	double time[N];
	int event_count = 0;
	int i;
	for ( i = 0; i < N; i++) {

		clock_gettime(CLOCK_REALTIME, &start);
		/* Enqueue DMA job */
		result = doca_workq_submit(state.workq, &dma_job.base);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(result));
			doca_buf_refcount_rm(dst_doca_buf, NULL);
			doca_buf_refcount_rm(src_doca_buf, NULL);
			doca_mmap_destroy(remote_mmap);
			free(dpu_buffer);
			dma_cleanup(&state, dma_ctx);
			return result;
		}

                event_count = epoll_wait(epoll_fd, events, 5, -1);
                //printf("%d ready events\n", event_count);
                if (event_count == -1) {
                        printf("epoll_wait error\n");
                        return 1;
                }

                result = doca_workq_event_handle_clear(state.workq, handle);
                if (result != DOCA_SUCCESS) {
                        DOCA_LOG_ERR("Failed clear WorkQ event handle: %s", doca_get_error_string(result));
                        return result;
                }

		/* Wait for job completion */
		while ((result = doca_workq_progress_retrieve(state.workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
		       DOCA_ERROR_AGAIN) {
			//ts.tv_nsec = SLEEP_IN_NANOS;
			//nanosleep(&ts, &ts);
		}
		clock_gettime(CLOCK_REALTIME, &end);

		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to retrieve DMA job: %s", doca_get_error_string(result));
			return result;
		}

		/* event result is valid */
		result = (doca_error_t)event.result.u64;
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("DMA job event returned unsuccessfully: %s", doca_get_error_string(result));
			return result;
		}

		ns = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
		//total_time_in_ns += ns;
		time[i] = ns;

		//DOCA_LOG_INFO("Remote DMA copy was done Successfully in %.2f us", ns/1000);
		//dpu_buffer[dst_buffer_size] = '\0';
		//DOCA_LOG_INFO("Memory content: %s", dpu_buffer);

                result = doca_workq_event_handle_arm(state.workq);
                if (result != DOCA_SUCCESS) {
                        DOCA_LOG_ERR("Unable to arm WorkQ: %s", doca_get_error_string(result));
                        return result;
                }

	}

	//DOCA_LOG_INFO("Memory content: %s", dpu_buffer);
	double min_t = time[0];
	double max_t = time[0];
	for (i = 0; i < N; i++) {
		if (time[i] <= min_t)
			min_t = time[i];
		if (time[i] >= max_t)
			max_t = time[i];
		total_time_in_ns += time[i];
	}
	double mean_t = (double)total_time_in_ns/N;
	double std_dev = 0;
	for (i = 0; i < N; i++) {
		std_dev += pow(abs(time[i]-mean_t),2);
	}
	std_dev = sqrt(std_dev/N);
        printf("Blocking DMA write\n");
        printf("Min time(us)\t Avg Lat(us)\t Max time(us)\t Std dev(us)\n");
        printf("%.2f\t %13.2f\t %13.2f\t %13.2f\n", min_t/1000, mean_t/1000, max_t/1000, std_dev/1000);

        if (close(epoll_fd)) {
                fprintf(stderr, "Failed to close epoll file descriptor\n");
                return 1;
        }

	if (doca_buf_refcount_rm(src_doca_buf, NULL) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove DOCA source buffer reference count");

	if (doca_buf_refcount_rm(dst_doca_buf, NULL) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove DOCA destination buffer reference count");

	/* Destroy remote memory map */
	if (doca_mmap_destroy(remote_mmap) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy remote memory map");

	/* Inform host that DMA operation is done */
	DOCA_LOG_INFO("Host sample can be closed, DMA copy ended");

	/* Clean and destroy all relevant objects */
	dma_cleanup(&state, dma_ctx);

	free(dpu_buffer);

	return result;
}
