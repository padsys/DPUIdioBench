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
 #include <doca_log.h>
 
 #include "dma_common.h"
 
 DOCA_LOG_REGISTER(DMA_COPY_DPU::MAIN);
 
 doca_error_t dma_copy_host(const char *export_desc_file_path,
                           const char *buffer_info_file_path,
                           const char *pcie_addr);
 
 int main(int argc, char **argv)
 {
     struct dma_config dma_conf;
     doca_error_t    result;
     struct doca_log_backend *sdk_log;
     int exit_status = EXIT_FAILURE;
 
     strcpy(dma_conf.pci_address,  "b1:00.0");
     strcpy(dma_conf.export_desc_path, "/tmp/export_desc.txt");
     strcpy(dma_conf.buf_info_path,    "/tmp/buffer_info.txt");
     dma_conf.cpy_txt[0] = '\0';
 
     /* standard log backend */
     if ((result = doca_log_backend_create_standard()) != DOCA_SUCCESS)
         goto sample_exit;
 
     /* SDK errors/warnings only */
     if ((result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log)) != DOCA_SUCCESS ||
         (result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_WARNING)) != DOCA_SUCCESS)
         goto sample_exit;
 
     DOCA_LOG_INFO("Starting the sample");
 
     if ((result = doca_argp_init("doca_dma_copy_host", &dma_conf)) != DOCA_SUCCESS ||
         (result = register_dma_params(true)) != DOCA_SUCCESS)
     {
         DOCA_LOG_ERR("ARGP init failed: %s", doca_error_get_descr(result));
         goto sample_exit;
     }
 
     if ((result = doca_argp_start(argc, argv)) != DOCA_SUCCESS) {
         DOCA_LOG_ERR("Failed to parse input: %s", doca_error_get_descr(result));
         goto argp_cleanup;
     }
 
     result = dma_copy_host(dma_conf.export_desc_path,
                           dma_conf.buf_info_path,
                           dma_conf.pci_address);
     printf("dma_copy_host() returned %d\n", result);
     if (result == DOCA_SUCCESS)
         exit_status = EXIT_SUCCESS;
 
 argp_cleanup:
     doca_argp_destroy();
 sample_exit:
     DOCA_LOG_INFO("Sample %s", exit_status == EXIT_SUCCESS ? "finished successfully" : "finished with errors");
     return exit_status;
 }
 