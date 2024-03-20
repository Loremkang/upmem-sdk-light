#pragma once

#include "pim_interface.hpp"

class UPMEMInterface : public PIMInterface {
    public:
    void SendToPIM(uint8_t** buffers, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        SendToPIMByUPMEM(buffers, symbol_name, symbol_offset, length, async_transfer);
    }

    void ReceiveFromPIM(uint8_t** buffers, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        ReceiveFromPIMByUPMEM(buffers, symbol_name, symbol_offset, length, async_transfer);
    }

    void Launch(bool async) {
        auto async_parameter = async ? DPU_ASYNCHRONOUS : DPU_SYNCHRONOUS;
        DPU_ASSERT(dpu_launch(dpu_set, async_parameter));
    }
};