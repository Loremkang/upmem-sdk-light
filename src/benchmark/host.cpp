#include <parlay/parallel.h>

#include <cassert>
#include <cstdio>
#include <iomanip>  // Add this line at the top of your file if it's not already there
#include <iostream>
#include <string>

#include "pim_interface_header.hpp"
#include "timer.hpp"
using namespace std;
void parse_arguments(int argc, char **argv, int &nr_ranks,
                     string &interfaceType, CommunicationDirection &direction) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <nr_ranks> <Interface Type> <Communication Direction>\n", argv[0]);
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

    string dir = argv[3];
    if (dir == "Host2PIM") {
        direction = CommunicationDirection::Host2PIM;
    } else if (dir == "PIM2Host") {
        direction = CommunicationDirection::PIM2Host;
    } else {
        fprintf(stderr,
                "Invalid communication direction. Please enter either 'Host2PIM' or "
                "'PIM2Host'.\n");
        exit(1);
    }
}

enum CommunicationDirection { Host2PIM = 0, PIM2Host = 1 };

void TestMRAMThroughput(PIMInterface *interface,
                        CommunicationDirection direction) {
    const int MinBufferSizePerDPU = 1 << 10;
    const int MaxBufferSizePerDPU = 1 << 20;
    double timeLimitPerTest = 2.0;  // 2 seconds

    int nr_of_dpus = interface->GetNrOfDPUs();

    uint8_t **dpuBuffer = new uint8_t *[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
        dpuBuffer[i] = new uint8_t[MaxBufferSizePerDPU];
    }

    internal_timer timer;
    for (size_t buffer_size_per_dpu = 1 << 10; buffer_size_per_dpu <= 1 << 20;
         buffer_size_per_dpu <<= 1) {
        timer.reset();
        for (uint64_t repeat = 0; true; repeat++) {
            parlay::parallel_for(0, nr_of_dpus, [&](size_t i) {
                parlay::parallel_for(0, buffer_size_per_dpu, [&](size_t j) {
                    dpuBuffer[i][j] = (uint8_t)j ^ 0x3f ^ repeat;
                });
            });

            timer.start();
            if (direction == CommunicationDirection::Host2PIM) {
                // CPU -> PIM.MRAM : Supported by both direct and UPMEM
                // interface.
                interface->SendToPIM(dpuBuffer, DPU_MRAM_HEAP_POINTER_NAME, 0,
                                     buffer_size_per_dpu, false);
            } else {
                // PIM.MRAM -> CPU : Supported by both direct and UPMEM
                // interface.
                interface->ReceiveFromPIM(dpuBuffer, DPU_MRAM_HEAP_POINTER_NAME,
                                          0, buffer_size_per_dpu, false);
            }
            timer.end();

            parlay::parallel_for(0, nr_of_dpus, [&](size_t i) {
                parlay::parallel_for(0, buffer_size_per_dpu, [&](size_t j) {
                    assert(dpuBuffer[i][j] == ((uint8_t)j ^ 0x3f ^ repeat));
                });
            });
            if (timer.total_time > timeLimitPerTest) {
                double bandwidth =
                    (double)buffer_size_per_dpu * repeat / timer.total_time;

                cout << "Buffer size: " << setw(10) << buffer_size_per_dpu
                     << " bytes, Repeat: " << setw(10) << repeat
                     << ", Time: " << setw(10) << timer.total_time
                     << " seconds, Bandwidth: " << setw(10) << bandwidth << " bytes/second"
                     << endl;

                break;
            }
        }
    }

    for (int i = 0; i < nr_of_dpus; i++) {
        delete[] dpuBuffer[i];
    }
    delete[] dpuBuffer;
}

int main(int argc, char **argv) {
    int nr_ranks;
    string interfaceType;
    CommunicationDirection direction;

    parse_arguments(argc, argv, nr_ranks, interfaceType, direction);

    // To Allocate: identify the number of RANKS you want, or use
    // DPU_ALLOCATE_ALL to allocate all possible.
    PIMInterface *pimInterface;
    if (interfaceType == "direct") {
        pimInterface = new DirectPIMInterface(nr_ranks, "dpu");
    } else {
        pimInterface = new UPMEMInterface(nr_ranks, "dpu");
    }
    // DirectPIMInterface pimInterface(DPU_ALLOCATE_ALL, "dpu");

    int nr_of_dpus = pimInterface->GetNrOfDPUs();
    uint8_t **dpuIDs = new uint8_t *[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
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
    for (int i = 0; i < nr_of_dpus; i++) {
        uint64_t *id = (uint64_t *)dpuIDs[i];
        assert(*id == (uint64_t)i);
    }

    // Execute : will call the UPMEM interface.
    pimInterface->Launch(false);
    pimInterface->PrintLog([](int i) { return (i % 100) == 0; });

    for (int i = 0; i < nr_of_dpus; i++) {
        delete[] dpuIDs[i];
    }
    delete[] dpuIDs;

    return 0;
}