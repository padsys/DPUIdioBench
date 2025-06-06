# /*
# * Copyright (c) 2025, University of California, Merced. All rights reserved.
# *
# * This file is part of the benchmarking software package developed by
# * the team members of Prof. Xiaoyi Lu's group at University of California, Merced.
# *
# * For detailed copyright and licensing information, please refer to the license
# * file LICENSE in the top level directory.
# *
# */

scp ubuntu@192.168.100.2:/opt/mellanox/doca/samples/doca_dma_test/blk_dma_write_dpu_thr/buf.txt .
scp ubuntu@192.168.100.2:/opt/mellanox/doca/samples/doca_dma_test/blk_dma_write_dpu_thr/desc.txt .
echo ""
./doca_dma_copy_host -d desc.txt -b buf.txt -p 01:00.0
