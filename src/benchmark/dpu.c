#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>

__host int64_t DPU_ID;

__host int64_t WRAM_TEST;
const int WRAM_BUFFER_SIZE = 10 << 10; // 10 KB
const int WRAM_BUFFER_SIZE_IN_INT64 = WRAM_BUFFER_SIZE / sizeof(uint64_t);
__host uint64_t wram_buffer[WRAM_BUFFER_SIZE_IN_INT64];

__host int64_t MRAM_TEST;
const int MRAM_BUFFER_SIZE = 60 << 20; // 60 MB
const int MRAM_BUFFER_SIZE_IN_INT64 = MRAM_BUFFER_SIZE / sizeof(uint64_t);
__mram uint8_t placeholder[1 << 20];
// __mram uint64_t buffer[MRAM_BUFFER_SIZE_IN_UINT64];

void StandardOutput() {
    printf("HEAP POINTER ADDR: %p\n", DPU_MRAM_HEAP_POINTER);
    printf("DPU ID is %lld!\n", DPU_ID);
    printf("DPU ID is at %p!\n", &DPU_ID);
}

void wram_test() {
    uint64_t offset = (DPU_ID << 48);
    for (int i = 0; i < WRAM_BUFFER_SIZE_IN_INT64; i ++) {
        wram_buffer[i] += offset + (uint64_t)(&wram_buffer[i]);
    }
    printf("WRAM: min=%16llx max=%16llx\n", wram_buffer[0], wram_buffer[WRAM_BUFFER_SIZE_IN_INT64 - 1]);
}

void mram_test() {
    uint64_t offset = (DPU_ID << 48);
    __mram_ptr uint64_t* mram_buffer = (__mram_ptr uint64_t*) DPU_MRAM_HEAP_POINTER;
    for (int i = 0; i < MRAM_BUFFER_SIZE_IN_INT64; i ++) {
        mram_buffer[i] += offset + (uint64_t)(&mram_buffer[i]);
    }
    printf("id=%lld\n", DPU_ID);
    // { 
    //     for (int i = 0; i < 2; i ++) {
    //         printf("%x %llx\n", mram_buffer + i, mram_buffer[i]);
    //     }
    //     printf("MRAM: min=%16llx max=%16llx\n", mram_buffer[0], mram_buffer[MRAM_BUFFER_SIZE_IN_INT64 - 1]);
    // } // will get random results because the memory isn't initialized
}

// __mram uint64_t val[1 << 20];

int main() {
    if (me() == 0) {
        StandardOutput();
    }
    
    // if (MRAM_TEST) {
    //     mram_test();
    // }
    // if (WRAM_TEST) {
    //     wram_test();
    // }

    return 0;
}
