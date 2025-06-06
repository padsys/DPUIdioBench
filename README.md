# DPUIdioBench
An Open-Source Benchmark Suite for Understanding the Idiosyncrasies of Emerging BlueField DPUs. This repository contains the code for our ICS'25 paper, "Arjun Kashyap, Yuke Li, Darren Ng, and Xiaoyi Lu. Understanding the Idiosyncrasies of Emerging BlueField DPUs".

Each folder represents experiments to test different aspects of DPU. The directory layout is as follows:
* dpudmabench/ contains the source for DPUDMABench, the micro-benchmark suite proposed in Section 3.3
* scripts/ contains scripts to configure DPU in different on-path submodes
* network/ contains the instructions to conduct on-path and off-path experiments for TCP and RDMA transports
* memory/ contains instructions to measure memory latency and memory bandwidth on the host and DPUs
* case_study contains instructions to run RDMA-based and TCP-based key-value stores (KVS) in on-path and off-path modes

DPU refers to NVIDIA's BlueField-1/BlueField-2/BlueField-3 i.e., any steps mentioned for DPU are applicable for BlueField-1, BlueField-2, and BlueField, unless otherwise stated. 

## Pre-requisites on host (x86)
* Linux x86_64 server running AlmaLinux 8.5 (Linux 4.18.0)
* gcc (tested with 8.5.0)
* Linux InfiniBand drivers (OFED -5.5 for BF-1, OFED-5.8 for BF-2, OFED-23.1 for BF-3)
* NVIDIA DOCA SDK (v1.5.0 for BF-2, v2.5.0 for BF-3) 
* iperf 3.5
* DPDK v21.08
* DPDK pktgen-21.02.0
* STREAM 5.10 
* tinymembench v0.4
* smhasher 
* HERD 
* MICA

## Pre-requisites on DPU 
* Ubuntu 20.04.5 (Linux 5.4.0-1049-bluefield) for BF1/2
* Ubuntu 22.04 (Linux 5.15-bluefield) for BF3
* gcc (tested with 9.4.0)
* Linux InfiniBand drivers (OFED -5.5 for BF-1, OFED-5.8 for BF-2, OFED-23.1 for BF-3)
* NVIDIA DOCA SDK (v1.5.0 for BF-2, v2.5.0 for BF-3)
* DPDK v21.08 
* STREAM 5.10
* tinymembench v0.4
* smhasher 
* HERD
* MICA

Note: RDMA perftest tool installed automatically with InfiniBand drivers

## DPDK installation on host/DPU
1.	Pre-requisite libraries:
    * Python v3.6.8
    * meson v0.61.5
    * ninja 1.10.2.git.kitware.jobserver-1
    * pyelftools v0.29
2.	Steps to install
```
git clone https://github.com/DPDK/dpdk.git
cd dpdk
git checkout tags/v21.08
mkdir x86_64-native-linux-gcc
meson -Dexamples=all --prefix=/home/<user>/dpdk/x86_64-native-linux-gcc build
```
where ```<user>``` indicates the name of the user directory on the machine. For DPU, the prefix could be ```--prefix=/home/<user>/dpdk/arm64-bluefield-linux-gcc-build build```. Then run the following - 
```
cd build
ninja
ninja install
sudo ldconfig
```
3. For all experiments, ensure the firewall is disabled on hosts as follows:
```
sudo systemctl stop firewalld
```
4. Assign a static IP address when the DPU’s link type is set to Ethernet. On DPU’s ethernet port (```enp1s0f0np0```) on host and client machines -
```
sudo ifconfig enp1s0f0np0 <some_static_ipv4_addr> netmask 255.255.255.0
```
Similarly, assign a static IP address to port (p0) on DPU - 
```
sudo ifconfig p0 <some_static_ipv4_addr> netmask 255.255.255.0
```

## Reference
Please cite DPUIdioBench in your publications if it helps your research:
```
@inproceedings{kashyap-ics25,
  author = {Kashyap, Arjun and Li, Yuke and Ng, Darren and Lu, Xiaoyi},
  title = {Understanding the Idiosyncrasies of Emerging
  BlueField DPUs},
  year = {2025},
  isbn = {979-8-4007-1537-2/2025/06},
  publisher = {Association for Computing Machinery},
  address = {New York, NY, USA},
  url = {https://doi.org/10.1145/3721145.3725780},
  doi = {10.1145/3721145.3725780},
  booktitle = {Proceedings of the 39th ACM International Conference on Supercomputing},
  numpages = {15},
  keywords = {Data Processing Unit (DPU), BlueField DPU, Performance},
  location = {Salt Lake City, UT, USA},
  series = {ICS '25}
}
```
