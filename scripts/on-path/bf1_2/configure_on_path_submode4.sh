#!/bin/bash
sudo ovs-vsctl del-br ovsbr1
sudo ovs-vsctl del-br br0-ovs

sudo ovs-vsctl add-br ovsbr1
sudo ovs-vsctl add-port ovsbr1 p0
sudo ovs-vsctl add-port ovsbr1 pf0hpf
sudo systemctl restart openvswitch-switch

# Enable hardware offload
sudo ethtool -K p0 hw-tc-offload on
sudo ethtool -K en3f0pf0sf0 hw-tc-offload on
sudo ethtool -K enp3s0f0s0 hw-tc-offload off
sudo ethtool -K pf0hpf hw-tc-offload on
sudo ovs-vsctl set Open_vSwitch . Other_config:hw-offload=true
sudo systemctl restart openvswitch-switch
