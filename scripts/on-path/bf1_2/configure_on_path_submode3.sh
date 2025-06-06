#!/bin/bash

# Delete any existing bridges
sudo ovs-vsctl del-br ovsbr1
sudo ovs-vsctl del-br br0-ovs
sudo systemctl stop openvswitch-switch

sudo echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
sudo /opt/mellanox/dpdk/bin/dpdk-testpmd -a 03:00.0,representor=[0,65535] -- -i -a --total-num-mbufs=131000
