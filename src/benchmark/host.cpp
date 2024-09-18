#include <parlay/parallel.h>

#include <cassert>
#include <cstdio>
#include <iomanip>  // Add this line at the top of your file if it's not already there
#include <iostream>
#include <string>
#include <sys/mman.h>

#include "pim_interface_header.hpp"
#include "timer.hpp"
using namespace std;

enum CommunicationDirection { Host2PIM = 0, PIM2Host = 1 };

void parse_arguments(int argc, char **argv, int &nr_ranks,
                     string &interfaceType) {
    if (argc < 3) {
        fprintf(
            stderr,
            "Usage: %s <nr_ranks> <Interface Type>\n",
            argv[0]);
        exit(1);
    }

    sscanf(argv[1], "%d", &nr_ranks);
    interfaceType = argv[2];

    if (interfaceType != "direct" && interfaceType != "UPMEM") {
        fprintf(stderr,
                "Invalid interface type. Please enter either 'direct' or "
                "'UPMEM'.\n");
        exit(1);
    }
}

void TestMRAMThroughput(PIMInterface *interface,
                        size_t MaxBufferSizePerDPU, bool flush_cache, bool no_huge_page) {
    const size_t MinTestSizePerDPU = 1 << 10;
    const size_t MaxTestSizePerDPU = std::min((size_t)1 << 20, MaxBufferSizePerDPU);
    const double timeLimitPerTest = 2.0;  // 2 seconds
    const double earlyStopTimeLimitPerTest = 1.0;
    const size_t repeatLimitPerTest = 500;

    int nrOfDPUs = interface->GetNrOfDPUs();

    size_t buffer_size = MaxBufferSizePerDPU * nrOfDPUs;
    size_t cacheline_size = 64;
    uint8_t *buffer = (uint8_t *)aligned_alloc(1l << 21, buffer_size);
    if (no_huge_page) {
        madvise(buffer, buffer_size, MADV_NOHUGEPAGE);
    }

    uint8_t **dpuBuffer = new uint8_t *[nrOfDPUs];

    for (int i = 0; i < nrOfDPUs; i++) {
        // void *ptr = malloc(MaxBufferSizePerDPU);
        // aligned_alloc(1l << 21, MaxBufferSizePerDPU);
        // madvise(ptr, MaxBufferSizePerDPU, MADV_NOHUGEPAGE);
        // dpuBuffer[i] = (uint8_t *)ptr;
        dpuBuffer[i] = buffer + i * MaxBufferSizePerDPU;
    }

    auto get_value = [&](size_t i, size_t j, uint64_t repeat) -> uint64_t {
        assert(i < (1 << 12));
        assert(j < (1 << 30));
        assert(repeat < (1 << 20));
        uint64_t combine = (i << 50) | (j << 20) | repeat;
        uint64_t val = parlay::hash64(combine);
        return val;
    };

    auto CheckFlushCache = [&](bool flush_cache, size_t size) {
        if (!flush_cache) {
            return;
        }
        while (size % cacheline_size != 0) {
            size++;
        }
        parlay::parallel_for(0, nrOfDPUs, [&](size_t dpu_id) {
            for (size_t i = 0; i < size; i += cacheline_size) {
                __builtin_ia32_clflushopt(dpuBuffer[dpu_id] + i);
            }
        });
    };

    internal_timer send_timer, recv_timer, total_timer;
    for (size_t bufferSizePerDPU = MinTestSizePerDPU;
         bufferSizePerDPU <= MaxTestSizePerDPU; bufferSizePerDPU <<= 1) {
        send_timer.reset();
        recv_timer.reset();
        total_timer.reset();
        total_timer.start();
        assert(bufferSizePerDPU % 4 == 0);

        for (size_t repeat = 0; true; repeat++) {
            parlay::parallel_for(0, nrOfDPUs, [&](size_t i) {
                parlay::parallel_for(0, bufferSizePerDPU / 8, [&](size_t j) {
                    uint64_t* ptr = (uint64_t*)(dpuBuffer[i] + j * 8);
                    *ptr = get_value(i, j, repeat);
                    // dpuBuffer[i][j] = get_value(i, j, repeat);
                });
            });

            CheckFlushCache(flush_cache, bufferSizePerDPU);

            send_timer.start();
            // CPU -> PIM.MRAM : Supported by both direct and UPMEM interface.
            interface->SendToPIM(dpuBuffer, DPU_MRAM_HEAP_POINTER_NAME, 0,
                                 bufferSizePerDPU, false);
            
            send_timer.end();

            interface->Launch(false);
            parlay::parallel_for(0, nrOfDPUs, [&](size_t i) {
                parlay::parallel_for(0, bufferSizePerDPU / 8, [&](size_t j) {
                    uint64_t* ptr = (uint64_t*)(dpuBuffer[i] + j * 8);
                    *ptr = get_value(i, j, repeat + 1);
                });
            });
            
            CheckFlushCache(flush_cache, bufferSizePerDPU);

            recv_timer.start();
            interface->ReceiveFromPIM(dpuBuffer, DPU_MRAM_HEAP_POINTER_NAME, 0,
                                      bufferSizePerDPU, false);
            recv_timer.end();

            parlay::parallel_for(0, nrOfDPUs, [&](size_t i) {
                parlay::parallel_for(0, bufferSizePerDPU / 8, [&](size_t j) {
                    uint64_t* ptr = (uint64_t*)(dpuBuffer[i] + j * 8);
                    assert(*ptr == get_value(i, j, repeat));
                });
            });

            if ((send_timer.total_time >= timeLimitPerTest &&
                 recv_timer.total_time >= timeLimitPerTest) ||
                (send_timer.total_time >= earlyStopTimeLimitPerTest &&
                 recv_timer.total_time >= earlyStopTimeLimitPerTest &&
                 repeat >= repeatLimitPerTest)) {
                total_timer.end();

                double send_bandwidth = (double)bufferSizePerDPU * repeat *
                                        nrOfDPUs / send_timer.total_time;
                double receive_bandwidth = (double)bufferSizePerDPU * repeat *
                                           nrOfDPUs / recv_timer.total_time;

                printf(
                    "CLFLUSH: %s, Total Buffer size: %5lu KB, Test Buffer "
                    "Size: %5lu KB, Repeat: %6lu, Send Time: %5lf s, Recv "
                    "Time: %5lf s, Total Time: %5lf s, "
                    "Send BW: %5lf GB/s, Recv BW: %5lf GB/s\n",
                    flush_cache ? "T" : "F", MaxBufferSizePerDPU / 1024,
                    bufferSizePerDPU / 1024, repeat, send_timer.total_time,
                    recv_timer.total_time, total_timer.total_time,
                    send_bandwidth / 1024.0 / 1024.0 / 1024.0,
                    receive_bandwidth / 1024.0 / 1024.0 / 1024.0);

                break;
            }
        }
    }

    delete[] dpuBuffer;
    free(buffer);
}

int main(int argc, char **argv) {
    int nr_ranks;
    string interfaceType;

    parse_arguments(argc, argv, nr_ranks, interfaceType);

    // To Allocate: identify the number of RANKS you want, or use
    // DPU_ALLOCATE_ALL to allocate all possible.
    PIMInterface *pimInterface;
    if (interfaceType == "direct") {
        pimInterface = new DirectPIMInterface(nr_ranks, "dpu");
    } else {
        pimInterface = new UPMEMInterface(nr_ranks, "dpu");
    }
    // DirectPIMInterface pimInterface(DPU_ALLOCATE_ALL, "dpu");

    int nrOfDPUs = pimInterface->GetNrOfDPUs();
    uint8_t **dpuIDs = new uint8_t *[nrOfDPUs];
    for (int i = 0; i < nrOfDPUs; i++) {
        dpuIDs[i] = new uint8_t[16];  // two 64-bit integers
        uint64_t *id = (uint64_t *)dpuIDs[i];
        *id = i;
    }

    // CPU -> PIM.WRAM : Not supported by direct interface. Use UPMEM interface.
    pimInterface->SendToPIMByUPMEM(dpuIDs, "DPU_ID", 0, sizeof(uint64_t),
                                   false);
    // following command will cause an error.
    // pimInterface.SendToPIM(dpuIDs, "DPU_ID", 0, sizeof(uint64_t), false);

    // PIM.WRAM -> CPU : Supported by both direct and UPMEM interface.
    pimInterface->ReceiveFromPIM(dpuIDs, "DPU_ID", 0, sizeof(uint64_t), false);
    for (int i = 0; i < nrOfDPUs; i++) {
        uint64_t *id = (uint64_t *)dpuIDs[i];
        assert(*id == (uint64_t)i);
    }

    // Execute : will call the UPMEM interface.
    pimInterface->Launch(false);
    pimInterface->PrintLog([](int i) { return (i % 100) == 0; });

    TestMRAMThroughput(pimInterface, (6400 - 4) << 10, false, false);
    TestMRAMThroughput(pimInterface, (6400 - 4) << 10, true, false);

    for (int i = 0; i < nrOfDPUs; i++) {
        delete[] dpuIDs[i];
    }
    delete[] dpuIDs;

    return 0;
}