#include <cstdio>
#include "pim_interface_header.hpp"
using namespace std;

int main() {

    // To Allocate: identify the number of RANKS you want, or use DPU_ALLOCATE_ALL to allocate all possible.
    DirectPIMInterface pimInterface(10, "dpu");
    // DirectPIMInterface pimInterface(DPU_ALLOCATE_ALL, "dpu");

    
    pimInterface.Launch(false);
    pimInterface.PrintLog([](int i){return true;});

    printf("Hello World!\n");
    return 0;
}