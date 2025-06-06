#!/bin/bash

# Delete any existing bridges -- ovsbr1 is always NAN, you should check using sudo ovs-vsctl list-br to get the exactly bridge name to delete in the following commands
sudo ovs-vsctl del-br ovsbr1 || true
sudo ovs-vsctl del-br br0-ovs || true
sudo systemctl stop openvswitch-switch

sudo ifconfig p0 mtu 1500
sudo ifconfig pf0hpf mtu 1500
sudo ifconfig en3f0pf0sf0 mtu 1500
sudo ifconfig enp3s0f0s0 mtu 1500

sudo sh -c 'echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'
sudo /opt/mellanox/dpdk/bin/dpdk-testpmd -a 03:00.0,representor=[0,65535] -- -i -a --total-num-mbufs=131000 --nb-cores=2
