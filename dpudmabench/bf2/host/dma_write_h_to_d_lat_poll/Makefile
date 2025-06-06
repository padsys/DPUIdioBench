CFLAGS  := -I . -I .. -I ../.. -I ../../.. -I ../../../.. -I /opt/mellanox/doca/include -I /usr/include/libnl3 -I /opt/mellanox/dpdk/include/dpdk -I /usr/include/json-c -fdiagnostics-color=always -D_FILE_OFFSET_BITS=64 -Wall -Winvalid-pch -g '-D DOCA_ALLOW_EXPERIMENTAL_API' -include rte_config.h -march=corei7 -mno-avx512f -include rte_config.h -march=corei7 -mno-avx512f -DALLOW_EXPERIMENTAL_API -include rte_config.h -march=corei7 -mno-avx512f
LD      := g++ -O3
LDFLAGS := ${LDFLAGS} -Wl,--as-needed -Wl,--no-undefined -Wl,-rpath,/opt/mellanox/doca/lib64 -Wl,-rpath-link,/opt/mellanox/doca/lib64 -Wl,--as-needed -Wl,--start-group /opt/mellanox/doca/lib64/libdoca_common.so -Wl,--as-needed /opt/mellanox/doca/lib64/libdoca_dma.so -Wl,--as-needed /opt/mellanox/doca/lib64/libdoca_argp.so /usr/lib64/libbsd.so -Wl,--end-group -lm

APPS    := doca_dma_copy_host

all: ${APPS}

doca_dma_copy_host: utils.o common.o dma_common.o  dma_copy_host_sample.o dma_copy_host_main.o
	${LD} -o $@ $^ ${LDFLAGS}

PHONY: clean
clean:
	rm -f *.o ${APPS}
