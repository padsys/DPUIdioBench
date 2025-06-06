#!/bin/bash

cores=$1
hostname=<server_ip_address>
logdir="logfile"
mkdir $logdir

for (( i=1; i <=$cores; i++ ));do
        port=510$i
        stream_name="s"$i
        logfile=$logdir$stream_name
        iperf3 -A $i,$i -c $hostname -t 20 -T $stream_name -p $port > $logfile &
done