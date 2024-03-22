#include <stdio.h>
#include <stdint.h>

#include "direct_interface.hpp"
using namespace std;

int main() {

    const int NR_RANKS = 1;

    DirectPIMInterface pimInterface(NR_RANKS, false);
    pimInterface.Load("dpu");

    int nr_of_dpus = pimInterface.GetNrOfDPUs();
    const int BUFFER_SIZE = 1024;
    uint8_t **input_buf = new uint8_t*[nr_of_dpus];
    uint64_t checksum[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
        input_buf[i] = new uint8_t[BUFFER_SIZE];
        for (int j = 0; j < BUFFER_SIZE; j++) {
            input_buf[i][j] = (uint8_t)(i + j);
        }

        checksum[i] = 0;
        uint64_t *array = (uint64_t*)input_buf[i];
        for (int j = 0; j < BUFFER_SIZE / 8; j++) {
            checksum[i] += array[j];
        }
    }

    const int OUT_BUFFER_SIZE = 8;
    uint8_t **output_buf = new uint8_t*[nr_of_dpus];
    for (int i = 0; i < nr_of_dpus; i++) {
        output_buf[i] = new uint8_t[OUT_BUFFER_SIZE];
    }

    // CPU -> PIM.MRAM : Supported by both direct and UPMEM interface.
    pimInterface.SendToPIM(input_buf, "input_buf", 0, BUFFER_SIZE);

    // Execute : will call the UPMEM interface.
    pimInterface.Launch();

    // PIM.MRAM -> CPU : Supported by both direct and UPMEM interface.
    pimInterface.ReceiveFromPIM(output_buf, "output_buf", 0, 8);

    // Check
    for (int i = 0; i < nr_of_dpus; i++) {
        uint64_t output_checksum = *(uint64_t*)output_buf[i];
        assert(checksum[i] == output_checksum);
        if (i % 10 == 0) {
            printf("DPU[%d] passed: %lu == %lu\n", i, checksum[i], output_checksum);
        }
    }

    for (int i = 0; i < nr_of_dpus; i++) {
        delete [] input_buf[i];
        delete [] output_buf[i];
    }
    delete [] input_buf;
    delete [] output_buf;

    return 0;
}