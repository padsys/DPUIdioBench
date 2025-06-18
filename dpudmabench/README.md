DPUDMABench is divided has two parts for measuring DMA performance on BF-2's and BF-3's DMA engine under bf2/ and bf3/ subfolders. The benchmark is based on the sample DMA code from the DOCA SDK. Each subfolder is further divided into two components: host and dpu. As their name suggests, each component runs on either the host or the DPU. Both components (host and dpu) need to be run in order to measure the DMA engine's performance. Each component contains code for different DMA reads/writes (event vs. polling and host vs. DPU initiated).

For latency tests, the number of iterations (N) is always 5000. For throughput test, in polling mode, the number of iterations (N) needs to be set to 5000 for payload of 2 bytes - 131072 bytes, 1000 for 262144 bytes - 1048576 bytes, and 100 for 2097152 bytes - 8388608 bytes. For throughput test, in event-based mode, the number of iterations (N) needs to be set to 1000 for payload of 2 bytes - 1048576 bytes and 100 for 2097152 bytes - 8388608 bytes.

#### Example

For instance, to measure the latency of DMA write (H-to-D) using polling for DMA completions on BF-2, the host side component is ```dpudmabench/bf2/host/host_dma_write_h_to_d_lat_poll``` and the DPU component is ```dpudmabench/bf2/dpu/dpu_dma_write_h_to_d_lat_poll```. We have provided a Makefile and a run.sh script under each folder to generate the binary for the specific DMA operation and help launch the component. Note that DPUDMABench works on BF-2/BF-3 since BF-1 does not expose the DOCA DMA engine.

To measure DMA write (H-to-D)(poll), first launch DPU side component-
```
dpudmabench/bf2/dpu/run.sh
```
The size of the DMA buffer should be provided in the run.sh file. The script first updates the buffer size and then compiles the code. The script file also shows an example of how one can copy the descriptor info (desc.txt) and buffer info (buf.txt) between the host-DPU as these files are required before initiating a DMA operation. Then it launches the host or DPU side component ```dpudmabench/bf2/dpu/dpu_dma_write_h_to_d_lat -p 03:00.0 -d desc.txt -b buf.txt```. 

For DMA write (H-to-D)(poll), the DPU side component generates desc.txt and buf.txt files. These files need to be transferred to the host (e.g., via scp) where the host-side component resides. After that launch host side component - ```dpudmabench/bf2/host/host_dma_write_h_to_d_lat -d desc.txt -b buf.txt -p <pcie_addr_dpu>```. For latency, the DPUDMABench performs 5000 iterations for a given buffer size for DMA and prints the latency on the host component.

Any component with the suffix “event” indicates DMA operations where event-based DMA completions are used. For example, the host and DPU components for event-based DMA are host_dma_write_h_to_d_lat_event and dpu_dma_write_h_to_d_lat_event, respectively. The performance of event-based DMA operation can be measured similarly to polling-based ones, as discussed above. 

Measuring DMA write (D-to-H) can be done by first running the host-side component and then the DPU-side component. Similarly, copy the buf.txt and desc.txt files from the host to the DPU (opposite to the H-to-D case) - 
```
host> dpudmabench/bf2/host/host_dma_write_d_to_h_lat -d desc.txt -b buf.txt -p 01:00.0
dpu> dpudmabench/bf2/dpu/dpu_dma_write_d_to_h_lat -p 03:00.0 -d desc.txt -b buf.txt
```
Note that the PCIe addresses might be different on different machines.

Similarly, throughput can be computed for various types of DMA operations. The suffix “thr” and “thr_event” represent the throughput of polling and event-based DMA operations, respectively.

For Figure 6a, the core utilization on the host and DPU is measured by the Linux perf utility.

For Figure 5(f)-5(i), the RDMA performance (throughput and latency) is measured by the RDMA perftest tool between the DPU and its host. Specifically, the performance of RDMA Read was measured by ```ib_read_lat``` and ```ib_read_bw``` while the performance of RDMA Write was measured by ```ib_write_lat``` and ```ib_write_bw```. For example, measuring the latency of RDMA Write (D-to-H), i.e., DPU-initiated RDMA Read operation, run the following on the host and DPU-
```
host> ib_read_lat -a -F
dpu> ib_read_lat -a -F <host>
```
where <host> represents the IP address of the host
Similarly, performance of RDMA operation can be measured and compared with polling-based DMA operations.

This experiment characterizes and compares the performance of different data exchange primitives between the host and the DPU—DMA and RDMA.
