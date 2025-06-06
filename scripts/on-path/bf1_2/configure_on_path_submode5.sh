#!/bin/bash

sudo systemctl start openvswitch-switch

# Delete any existing bridges on DPU
sudo ovs-vsctl del-br ovsbr1
sudo ovs-vsctl del-br br0-ovs

# Configure OvS-DPDK
sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-init=true
sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:hw-offload=true
sudo ovs-vsctl set Open_vSwitch . other_config:dpdk-extra="-w 0000:03:00.0,representor=[0,65535],dv_flow_en=1,dv_xmeta_en=1,sys_mem_en=1"
sudo ovs-vsctl add-br br0-ovs -- set Bridge br0-ovs datapath_type=netdev -- br-set-external-id br0-ovs bridge-id br0-ovs -- set bridge br0-ovs fail-mode=standalone
sudo ovs-vsctl add-port br0-ovs p0 -- set Interface p0  type=dpdk options:dpdk-devargs=0000:03:00.0
sudo ovs-vsctl add-port br0-ovs pf0hpf -- set Interface pf0hpf type=dpdk options:dpdk-devargs=0000:03:00.0,representor=[0,65535]
sudo systemctl restart openvswitch-switch

sudo ovs-ofctl -O OpenFlow12 add-flow ovsdpdk "ip, in_port=p0, ip_dst=169.236.0.78, actions=output:pf0hpf"
sudo ovs-ofctl -O OpenFlow12 add-flow ovsdpdk "ip, in_port=pf0hpf, ip_dst=169.236.0.159, actions=output:p0"