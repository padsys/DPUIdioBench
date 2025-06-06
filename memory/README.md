## Experiment for Figure 6(b) (Memory latency)
1.	Download and install tinymembench on host, BF-1, BF-2, and BF-3
```
git clone https://github.com/ssvb/tinymembench
cd tinymembench
git checkout tags/v0.4
make
```
2. Run tinymembench on one of the host, BF-1, BF-2, and BF-3:
```
./tinymembench
```

The benchmark reports memory latency for various memory blocks with both hugepages enabled and disabled.


## Experiment for Table 4 (Memory bandwidth)
1. Download *stream.c* file from STREAM on host, BF-1, BF-2, and BF-3 -
```
wget https://www.cs.virginia.edu/stream/FTP/Code/stream.c
```
2. Compile and run the single-core version as -
```
gcc stream.c -o stream
./stream
```

3. For running the multi-core version as -
```
gcc -fopenmp -D_OPENMP stream.c -o stream_multi
OMP_NUM_THREADS=<N> ./stream_multi
```
where <N> is 12, 16, 8, and 8 for our host, BF-1, BF-2, and BF-3, respectively.

The output of the STREAM benchmark reports memory bandwidth.
