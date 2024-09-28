rm -rf ./build
mkdir build
cd build
cmake ..
make -j

numactl --interleave=all ./benchmark 32 direct | tee ../results/direct_1.txt
echo "direct 1 done"
numactl --interleave=all ./benchmark 32 direct | tee ../results/direct_2.txt
echo "direct 2 done"
numactl --interleave=all ./benchmark 32 direct | tee ../results/direct_3.txt
echo "direct 3 done"

numactl --interleave=all ./benchmark 32 UPMEM | tee ../results/UPMEM_1.txt
echo "UPMEM 1 done"
numactl --interleave=all ./benchmark 32 UPMEM | tee ../results/UPMEM_2.txt
echo "UPMEM 2 done"
numactl --interleave=all ./benchmark 32 UPMEM | tee ../results/UPMEM_3.txt
echo "UPMEM 3 done"
