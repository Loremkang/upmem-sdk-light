#include <cstdio>
#include <parlay/parallel.h>

#include "pim_interface_header.hpp"
using namespace std;

int main() {

    const int NR_RANKS = 10;

    // To Allocate: identify the number of RANKS you want, or use DPU_ALLOCATE_ALL to allocate all possible.
    DirectPIMInterface pimInterface(NR_RANKS, "dpu_example");
    // DirectPIMInterface pimInterface(DPU_ALLOCATE_ALL, "dpu");

    int nr_of_dpus = pimInterface.GetNrOfDPUs();
    uint8_t **dpuIDs = new uint8_t*[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
        dpuIDs[i] = new uint8_t[16]; // two 64-bit integers
        uint64_t *id = (uint64_t *)dpuIDs[i];
        *id = i;
    }

    // CPU -> PIM.WRAM : Not supported by direct interface. Use UPMEM interface.
    pimInterface.SendToPIMByUPMEM(dpuIDs, 0, "DPU_ID", 0, sizeof(uint64_t), false);
    // following command will cause an error.
    // pimInterface.SendToPIM(dpuIDs, "DPU_ID", 0, sizeof(uint64_t), false);

    // PIM.WRAM -> CPU : Supported by both direct and UPMEM interface.
    pimInterface.ReceiveFromPIM(dpuIDs, 0, "DPU_ID", 0, sizeof(uint64_t), false);
    for (int i = 0; i < nr_of_dpus; i++) {
        uint64_t *id = (uint64_t *)dpuIDs[i];
        assert(*id == (uint64_t)i);
    }

    const int BUFFER_SIZE = 4 << 20;
    uint8_t **dpuBuffer = new uint8_t*[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
        dpuBuffer[i] = new uint8_t[BUFFER_SIZE]; // 4 MB buffer
    }
    parlay::parallel_for(0, nr_of_dpus, [&](size_t i) {
        parlay::parallel_for(0, BUFFER_SIZE, [&](size_t j) {
            dpuBuffer[i][j] = (uint8_t)j ^ 0x3f;
        });
    });

    // CPU -> PIM.MRAM : Supported by both direct and UPMEM interface.
    pimInterface.SendToPIM(dpuBuffer, 0, DPU_MRAM_HEAP_POINTER_NAME, 0, BUFFER_SIZE, false);

    // PIM.MRAM -> CPU : Supported by both direct and UPMEM interface.
    pimInterface.ReceiveFromPIM(dpuBuffer, 0, DPU_MRAM_HEAP_POINTER_NAME, 0, BUFFER_SIZE, false);

    parlay::parallel_for(0, nr_of_dpus, [&](size_t i) {
        parlay::parallel_for(0, BUFFER_SIZE, [&](size_t j) {
            assert(dpuBuffer[i][j] == ((uint8_t)j ^ 0x3f));
        });
    });

    // Execute : will call the UPMEM interface.
    pimInterface.Launch(false);
    pimInterface.PrintLog([](int i){return (i % 100) == 0;});

    for (int i = 0; i < nr_of_dpus; i++) {
        delete [] dpuIDs[i];
        delete [] dpuBuffer[i];
    }
    delete [] dpuBuffer;
    delete [] dpuIDs;

    return 0;
}