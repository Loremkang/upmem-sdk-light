#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <assert.h>

__host int64_t DPU_ID;

void StandardOutput() {
    printf("HEAP POINTER ADDR: %p\n", DPU_MRAM_HEAP_POINTER);
    printf("DPU ID is %lld!\n", DPU_ID);
    printf("DPU ID is at %p!\n", &DPU_ID);
}

#define BUFFER_SIZE (32)
void PrintMRAMHeap(int size) {
    assert(size <= BUFFER_SIZE);
    uint64_t buffer[BUFFER_SIZE];
    mram_read(DPU_MRAM_HEAP_POINTER, buffer, sizeof(uint64_t) * BUFFER_SIZE);
    for (int i = 0; i < size; i ++) {
        printf("%d %lx\n", i, buffer[i]);
    }
}

int main() {
    if (me() == 0) {
        StandardOutput();
        PrintMRAMHeap(4);
    }
    return 0;
}
