#!/bin/bash

cores=$1

pkill iperf3

for (( i=1; i<=$cores; i++ ));do
        port="510"$i
        iperf3 -s -D -p $port &
done