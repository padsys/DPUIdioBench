CFLAGS  := -I. -I.. -I../.. -I../../.. -I../../../.. -I../../../../applications/common/src -I/opt/mellanox/doca/include -I/opt/mellanox/dpdk/include/dpdk -I/opt/mellanox/dpdk/include/dpdk/../aarch64-linux-gnu/dpdk -I/usr/include/libnl3 -I/usr/include/json-c -fdiagnostics-color=always -D_FILE_OFFSET_BITS=64 -Wall -Winvalid-pch '-D DOCA_ALLOW_EXPERIMENTAL_API' -include rte_config.h -mcpu=cortex-a72 -include rte_config.h -mcpu=cortex-a72 -include rte_config.h -mcpu=cortex-a72 -DALLOW_EXPERIMENTAL_API
LD      := gcc -O2
LDFLAGS := ${LDFLAGS} -Wl,--as-needed -Wl,--no-undefined -Wl,-rpath,/opt/mellanox/doca/lib/aarch64-linux-gnu -Wl,-rpath-link,/opt/mellanox/doca/lib/aarch64-linux-gnu -Wl,--as-needed -Wl,--start-group /opt/mellanox/doca/lib/aarch64-linux-gnu/libdoca_common.so -Wl,--as-needed /opt/mellanox/doca/lib/aarch64-linux-gnu/libdoca_dma.so -Wl,--as-needed /opt/mellanox/doca/lib/aarch64-linux-gnu/libdoca_argp.so /usr/lib/aarch64-linux-gnu/libbsd.so -Wl,--end-group -lm

APPS    := doca_dma_copy_host

all: ${APPS}

doca_dma_copy_host: utils.o common.o dma_common.o  dma_copy_host_sample.o dma_copy_host_main.o
	${LD} -o $@ $^ ${LDFLAGS}

PHONY: clean
clean:
	rm -f *.o ${APPS}
