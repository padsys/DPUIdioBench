## Experiments for Figure 3 (Host-to-Host bandwidth in different on-path DPU modes)

The bandwidth between hosts is measured by configuring the DPU on one host into five different on-path submodes. The scripts to configure each submode can be found under the scripts/on-path folder. One DPU on one host is configured to different on-path modes, whereas the other DPU on another host is configured in off-path mode.
1. Configure DPU on one host in on-path mode from the host and set the link type to Ethernet, and power cycle the host. Note that the device name might be different. You can check the device name by running ```mst status -v```.
```
sudo mst start
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=1
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=2
```

2. On the host with DPU in off-path mode and set the link type to Ethernet, and power cycle the host.
```
sudo mst start
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=0
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=2
```

3. To configure DPU’s on-path submode<N> where N=1,2,3,4,5 by running the following on the DPU. For example, to configure DPU in on-path submode1, run the following-
```
sudo scripts/configure_on_path_submode1.sh
```
4. Using the kernel TCP stack on the host
On the host with DPU in on-path submodes, run iperf as follows-
```
scripts/on-path/iperf_server.sh <cores>
```
On another host, run iperf as follows-
```
scripts/on-path/iperf_clients.sh <cores>
```
where <cores> vary from 1 to 6 (inclusive) to represent varying cores

The bandwidth measured represents host-to-host bandwidth and shows the overhead of different on-path DPU submodes. Repeat step #4 by changing different on-path submodes as shown in step #3.

5. Using userspace TCP stack on the host
Install DPDK pktgen on hosts
```
git clone https://github.com/pktgen/Pktgen-DPDK.git
cd Pktgen-DPDK
git checkout tags/pktgen-21.02.0
export RTE_SDK=/home/<user>/dpdk/
export RTE_TARGET=x86_64-native-linux-gcc
make
```

For example, to launch pktgen with 6 host cores, launch pktgen as follows on the host with the DPU set in off-path mode-
```
host1> sudo LD_LIBRARY_PATH="/home/<user>/dpdk/build/lib" ./usr/local/bin/pktgen -a 0000:01:00.0 -l 0-6 -n 4 --socket-mem="2048" -- -T -P -j -m [1:1-6].0
```
The *host1* assigns 6 cores to transmit packets.

For example, to launch pktgen with 6 host cores, launch pktgen as follows on the host with different on-path submodes based on step #3-
```
host2> sudo LD_LIBRARY_PATH="/home/<user>/dpdk/build/lib" ./usr/local/bin/pktgen -a 0000:01:00.0 -l 0-6 -n 4 --socket-mem="1024" -- -T -P -j -m [1-6:1].0
```
The *host2* assigns 6 cores to receive packets.

Here, <user> indicates the name of the user directory on the machine and modify parameter value -m to increase the number of host cores from 1 to 6. Before transmitting packets from *host1*, set the destination MAC, IP address, and packet size to 1025 on *host2*. One can observe different bandwidths on *host2*, based on different on-path submodes.

This experiment shows the overheads of different DPU on-path submodes on the host.


## Experiment for Figure 4 (RDMA performance)

1. Configure DPU based on off-path and on-path and IB/RoCE
```
sudo mst start
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=1  # for on-path
```
or
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=0  # for off-path
```

For IB,
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=1
sudo opensm
```
For RoCE,
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=2
```
    
2.	Latency numbers for different RDMA paths are taken by ```ib_send_lat```, ```ib_read_lat```, and ```ib_write_lat``` for RDMA Send/Recv, RDMA Read, and RDMA Write, respectively. Bandwidth numbers for different RDMA paths are taken by ```ib_send_bw```, ```ib_read_bw```, and ```ib_write_bw``` for RDMA Send/Recv, RDMA Read, and RDMA Write, respectively. MTU is always 4096.
For example, for measuring RDMA Send/Recv latency of CD-SH_IB (client on DPU and server on host) path, the server command (on host) is-
```
ib_send_lat -a -F
```
while client command (on DPU) is-
```
ib_send_lat -a -F <host>
```
where <host> represents the IP address of the other host where the server command is running.

Similarly, for measuring RDMA Send/Recv bandwidth of CD-SH_IB path, the server command (on host) is-
```
ib_send_bw -a -F --report_gbits
```
while client command (on DPU) is-
```
ib_send_bw -a -F <host>
```
where <host> represents the IP address of the other host where the server command is running.

This experiment shows the latency of different RDMA paths between hosts and DPUs for two DPU modes.
