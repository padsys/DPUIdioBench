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

len=$1
sed -i "/length =/c\  length = ${len};" dma_copy_dpu_main.c
make clean && make
echo ""

./doca_dma_copy_dpu -p 03:00.0
