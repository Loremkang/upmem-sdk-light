#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <assert.h>

#define BUFFER_SIZE (1024 / sizeof(uint64_t))
__mram_noinit uint64_t input_buf[BUFFER_SIZE];
__mram_noinit uint64_t output_buf;

int main() {
    if (me() == 0) {
        uint64_t checksum = 0;
        __dma_aligned uint64_t wram_buf;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            mram_read(&input_buf[i], &wram_buf, 8);
            checksum += wram_buf;
        }

        wram_buf = checksum;
        mram_write(&wram_buf, &output_buf, 8);
    }
    return 0;
}
