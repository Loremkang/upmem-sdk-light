#pragma once

#include "direct_interface.hpp"
#include "upmem_interface.hpp"

#include <string>

namespace namespace_pim_interface {

    PIMInterface *pimInterface = NULL;

    void pim_interface_init(dpu_set_t dpu_set, std::string interfaceType) {
        if(pimInterface == NULL) {
            if(interfaceType == "direct") {
                pimInterface = new DirectPIMInterface(dpu_set);
                printf("Direct interface to DPUs\n");
            } else {
                pimInterface = new UPMEMInterface(dpu_set);
                printf("UPMEM interface to DPUs\n");
            }
        }
    }

    void pim_interface_delete() { if(pimInterface != NULL) delete pimInterface; }

    void SendToPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        pimInterface->SendToPIM(buffers, buffer_offset, symbol_name, symbol_offset, length, async_transfer);
    }

    void ReceiveFromPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name, uint32_t symbol_offset, uint32_t length, bool async_transfer) {
        pimInterface->ReceiveFromPIM(buffers, buffer_offset, symbol_name, symbol_offset, length, async_transfer);
    }

    void load_from_dpu_set(dpu_set_t dpu_set) {
        pimInterface->load_from_dpu_set(dpu_set);
    }

    void do_not_free_dpu_set_when_delete() {
        pimInterface->do_not_free_dpu_set_when_delete();
    }
};