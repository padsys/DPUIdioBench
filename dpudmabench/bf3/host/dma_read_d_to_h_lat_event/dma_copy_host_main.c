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

#include <stdlib.h>
#include <string.h>

#include <doca_argp.h>
#include <doca_dev.h>
#include <doca_log.h>

#include <utils.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_HOST::MAIN);

/* Sample's Logic */
doca_error_t dma_copy_host(struct doca_pci_bdf *pcie_addr, char *src_buffer, size_t src_buffer_size,
				  char *export_desc_file_path, char *buffer_info_file_path);

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int
main(int argc, char **argv)
{
	struct dma_config dma_conf = {0};
	char *src_buffer;
	size_t length;
	doca_error_t result;
	struct doca_log_backend *sdk_log;
	int exit_status = EXIT_FAILURE;

	/* Set the default configuration values (Example values) */
	strcpy(dma_conf.pci_address, "b1:00.0");
	strcpy(dma_conf.cpy_txt, "This is a sample piece of text");
	strcpy(dma_conf.export_desc_path, "/tmp/export_desc.txt");
	strcpy(dma_conf.buf_info_path, "/tmp/buffer_info.txt");

	/* Register a logger backend */
	result = doca_log_backend_create_standard();
	if (result != DOCA_SUCCESS)
		goto sample_exit;

	/* Register a logger backend for internal SDK errors and warnings */
	result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
	if (result != DOCA_SUCCESS)
		goto sample_exit;
	result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_WARNING);
	if (result != DOCA_SUCCESS)
		goto sample_exit;

	DOCA_LOG_INFO("Starting the sample");

	result = doca_argp_init("doca_dma_copy_host", &dma_conf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_error_get_descr(result));
		goto sample_exit;
	}
	result = register_dma_params(true);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register DMA sample parameters: %s", doca_error_get_descr(result));
		goto argp_cleanup;
	}
	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_error_get_descr(result));
		goto argp_cleanup;
	}

#ifndef DOCA_ARCH_HOST
	DOCA_LOG_ERR("Sample can run only on the Host");
	goto sample_exit;
#endif
  length = 2097152;
	src_buffer = (char *)malloc(length);
	if (src_buffer == NULL) {
		DOCA_LOG_ERR("Source buffer allocation failed");
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	//memcpy(src_buffer, dma_conf.cpy_txt, length);
	memset(src_buffer, '1', length);

	result = dma_copy_host(dma_conf.pci_address, src_buffer, length, dma_conf.export_desc_path, dma_conf.buf_info_path);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("dma_copy_host() encountered an error: %s", doca_error_get_descr(result));
		goto src_buf_cleanup;
	}

	exit_status = EXIT_SUCCESS;

src_buf_cleanup:
	if (src_buffer != NULL)
		free(src_buffer);
argp_cleanup:
	doca_argp_destroy();
sample_exit:
	if (exit_status == EXIT_SUCCESS)
		DOCA_LOG_INFO("Sample finished successfully");
	else
		DOCA_LOG_INFO("Sample finished with errors");
	return exit_status;
}
