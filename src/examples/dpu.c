#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>

__host int64_t DPU_ID;

void standard_output() {
    printf("HEAP POINTER ADDR: %p\n", DPU_MRAM_HEAP_POINTER);
    printf("&DPU_ID: %p\n", &DPU_ID);
    printf("Hello World from %lld!\n", DPU_ID);
}

int main() {
    standard_output();
    return 0;
}
