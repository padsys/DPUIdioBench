#### Experiment for Figure 8 (Off-path HERD KVS study)

1.	Configure DPUs on both machines to link type as IB and off-path mode. Run the following from the hosts and then power cycle the host. Note that the device name might be different. You can check the device name by running ```mst status -v```.
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=0  
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=1
```

2.	Download [HERD](https://github.com/efficient/rdma_bench) and the required packages and ensure hugepages can be enabled on both the hosts and DPUs.
3.	Modify the ```NUM_WORKERS``` in <herd_repo>/herd/main.h based on the number of server threads used in Figure 8.
4.	Modify ```NUM_CLIENTS``` in <herd_repo>/herd/main.h and num_threads in <herd_repo>/herd/run-machine.sh based on whether the client is running on the host or the DPU (in our case 3/6/9/12/16 for host, 4/8/12/16/20 for BF-1, and 1/2/4/8/12 for BF-2/3).
5.	Set the postlist in <herd_repo>/herd/run_server.sh to the number of client threads set in ( step #4).
6.	To measure latency, we added the code on the <herd_repo>/herd/client.c. The modifications to client.c are shown as a diff in the benchmarking/case_study/herd_client_diff file.
7.	Install HERD (```make```).
8.	Then perform the required settings for HERD -
```
sudo echo 6192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
sudo bash -c "echo kernel.shmmax = 9223372036854775807 >> /etc/sysctl.conf"
sudo bash -c "echo kernel.shmall = 1152921504606846720 >> /etc/sysctl.conf"
sudo sysctl -p /etc/sysctl.conf
```
9.	To generate 100% GET, 95% GET, and 50% GET workloads, modify ‘update-percentage’ in <herd_repo>/herd/run-machine.sh to 0, 5, and 50, respectively.
10.	To run the ```CD-SH (12)``` configuration, start the HERD server on the host and client on the other host's DPU. Here, the number in parentheses denotes the number of HERD server threads-
```
host1> ./run-server.sh
dpu2> ./run-machine.sh 0
```

11. For the ```CDH-SH (12)``` configuration, start the HERD server on one host. Start the HERD client on both another host and its DPU. The ```NUM_CLIENTS``` in <herd_repo>/herd/main.h on the HERD server should be set to the total number of clients on the host and DPU for the CDH-SH case.
```
# server
host1> ./run-server.sh
# clients
dpu2> ./run-machine 0
host2> ./run-machine 1
```

Vary the client threads by modifying based on step #4 to measure latency vs. throughput.

#### Experiment for Figure 9 (Off-path MICA KVS study)

1.	Configure DPUs on both machines to link type as Ethernet and off-path mode. Run the following from hosts and power cycle the host-
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=0  
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s LINK_TYPE_P1=2
```
2.	Etcd installation
On host
```
wget https://github.com/etcd-io/etcd/releases/download/v2.3.0/etcd-v2.3.0-linux-amd64.tar.gz
tar -xvzf etcd-v2.3.0-linux-amd64.tar.gz
```
On DPU
```
wget https://github.com/etcd-io/etcd/releases/download/v3.2.1/etcd-v3.2.1-linux-arm64.tar.gz 
tar -xvzf etcd-v3.2.1-linux-arm64.tar.gz
```
3.	Start etcd on the server machine.
```
cd etcd-v2.3.0-linux-amd64/
./etcd --initial-advertise-peer-urls http://<ip_addr>:2380 --listen-peer-urls http://<ip_addr>:2380 --listen-client-urls http://<ip_addr>:2379,http://127.0.0.1:2379 --advertise-client-urls http:// <ip_addr>:2379
```
where <ip_addr> refers to the IPv4 address of the host interface visible to the client machine.
4. On DPU (needed for DPU-only) the <ip_addr> refers to the IPv4 address of the out-of-band interface of DPU visible to the client machine.
```
cd etcd-v3.2.1-linux-arm64/
ETCD_UNSUPPORTED_ARCH=arm64 ./etcd --initial-advertise-peer-urls http://<ip_addr>:2380 --listen-peer-urls http://<ip_addr>:2380 --listen-client-urls http://<ip_addr>:2379,http://127.0.0.1:2379 --advertise-client-urls http://<ip_addr>:2379
```
5.	Download and build [MICA](https://github.com/efficient/mica2) and set up hugepages on server, client, and DPUs:
```
sudo <mica_repo>/script/setup.sh 6392
```
6.	Modify config files for server, client, and DPU. When the KVS server is on the host, edit ```mac_addr``` and ```ipv4_addr``` in server.json to the MAC and IP address of the ethernet interface (enp1s0f0np0). Edit ```etcd_addr`` to the IP address of where etcd is running.

7. When the KVS server is on DPU, edit ```mac_addr``` and ```ipv4_addr``` in server.json to the MAC and IP address of the ethernet interface (p0). Edit ```etcd_addr``` to the IP address of where etcd is running.
    
8. When the KVS client is on host/DPU, set ```etcd_addr```, ```mac_addr```, and ```ipv4_addr``` in netbench.json to point to the IPv4 address of the etcd running on either the server or the DPU, the MAC and IP address of the ethernet interface (enp1s0f0np0) of the client machine, respectively.

9.	To run the ```CH-SH (12)``` configuration, start the MICA server on the host. A sample config file for ```CH-SH (12)``` used for the MICA server and MICA client can be found under ```benchmarking/case_study/mica_server.json``` and ```benchmarking/case_study/mica_netbench.json```, respectively. The mica_server.json and mica_netbench.json configure the MICA server and the MICA client to run on 12 threads. These files can be modified to change the number of cores used by MICA KVS. To start the MICA server on one host and the client on another host -
```
sudo  mica2/build/server
sudo mica2/build/netbench 0.00
```
10. For the ```CDH-SH (12)``` configuration, start the MICA server on one host. Start the MICA client on both another host and its DPU.
```
#server
host1> sudo  mica2/build/server
#clients
dpu2> sudo mica2/build/netbench 0.00
host2> sudo mica2/build/netbench 0.00
```
11.	To generate 100% GET, 95% GET, and 50% GET workloads, modify ```get_ratio``` in mica_netbench.json to 1.00, 0.95, and 0.5, respectively.

Vary the client/server threads by modifying ‘lcores’ in netbench.json/server.json to measure latency vs. throughput.

#### Experiment for Figure 10 (Hash performance on host and DPU)
1.	Download and install smhasher on the host and DPU-
```
git clone https://github.com/rurban/smhasher
cd smhasher/src
mkdir build && cd build
cmake ..
make
```
2.	For example, to run Murmur3 of 128-bit width, run the following on host/DPU-
```
./SMHasher Murmur3C
```

The speed test reports the hash bandwidth for 256KB input and cycles/hash for 16-byte input. We convert cycles/hash to latency based on the time period of a single clock cycle on the respective core (host, BF-1, BF-2, or BF-3). The time period (1/frequency) for host, BF-1, BF-2 and BF-3 on our testbed is 0.3ns, 1.25ns, 0.4ns and 0.33ns, respectively.

The mapping between different hash functions and their widths to hash names can be found under smhasher/main.cpp. Hence, the performance of different hash functions can be tested via smhasher.

#### Experiment for Figure 11 (On-path KVS study)
1.	Configure DPU’s on-path submode<N> where N=1,2,3,4,5 by running the following on the DPU. For example, to configure DPU in on-path submode3, run the following on one of the hosts-
```
sudo benchmarking/scripts/on-path/bf3/configure_on_path_submode3.sh
```
2.	Configure DPU in off-path mode on another host and power cycle the host-
```
sudo mlxconfig -d /dev/mst/mt41686_pciconf0 s INTERNAL_CPU_MODEL=0
```
4.	Follow steps 1-8 from the off-path MICA KVS study to install and set up MICA KVS on both hosts.
5.	Run MICA server on the host where DPU is in on-path submode and MICA client on the other host where DPU is in off-path mode.
6.	To generate 100% GET, 95% GET, and 50% GET workloads, modify ```get_ratio``` in mica_netbench.json to 1.00, 0.95, and 0.5, respectively.

Vary the client/server threads by modifying ```lcores``` in netbench.json/server.json and DPU’s on-path submodes 1-5 to measure latency vs. throughput for different submodes.
