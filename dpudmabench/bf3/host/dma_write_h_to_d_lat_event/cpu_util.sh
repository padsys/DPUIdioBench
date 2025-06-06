#!/bin/bash

# Check if a program name is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <program_name> [program_arguments]"
    exit 1
fi

PROGRAM=$1
shift
ARGS=$@

# Run perf and process its output
perf stat -e cycles,instructions -I 1000 -x, $PROGRAM $ARGS 2>&1 | awk '
BEGIN {
    FS=","
}
/cycles/ {
    cycles=$1
}
/instructions/ {
    instructions=$1
    if (cycles > 0) {
        ipc = instructions / cycles
        utilization = ipc * 100
        if (utilization > 100) utilization = 100
        printf "CPU Utilization: %.2f%%\n", utilization
    }
}'
