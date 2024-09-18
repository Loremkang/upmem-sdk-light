#pragma once

#include "pim_interface.hpp"

class UPMEMInterface : public PIMInterface {
public:
    void SendToPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        SendToPIMByUPMEM(buffers, buffer_offset, symbol_name, symbol_offset, length, async_transfer);
    }

    void ReceiveFromPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        ReceiveFromPIMByUPMEM(buffers, buffer_offset, symbol_name, symbol_offset, length, async_transfer);
    }

    void Launch(bool async) {
        auto async_parameter = async ? DPU_ASYNCHRONOUS : DPU_SYNCHRONOUS;
        DPU_ASSERT(dpu_launch(dpu_set, async_parameter));
    }

    UPMEMInterface(int nr_ranks, std::string dpu_program) : PIMInterface(nr_ranks, dpu_program) {

    };
    
    UPMEMInterface(dpu_set_t dpu_set) : PIMInterface(dpu_set) {

    };
};
