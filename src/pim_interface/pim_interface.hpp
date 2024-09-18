#pragma once

#include <cassert>
#include <cstdio>
#include <string>

extern "C" {
#include <dpu.h>
#include <dpu_rank.h>
}

const uint32_t MAX_NR_RANKS = 40;
const uint32_t DPU_PER_RANK = 64;
const uint64_t MRAM_SIZE = (64 << 20);

class PIMInterface {
public:
    virtual void load_from_dpu_set(dpu_set_t dpu_set) {
        this->dpu_set = dpu_set;
        DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &this->nr_of_dpus));
        DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &this->nr_of_ranks));
        std::printf("Interface detected %d DPU(s)\n", this->nr_of_dpus);
        std::printf("Interface detected %d Ranks(s)\n", this->nr_of_ranks);
        assert(this->nr_of_dpus <= nr_of_ranks * DPU_PER_RANK);
    }

    PIMInterface(uint32_t nr_of_ranks, std::string binary): free_dpu_set_when_delete(true) {
        dpu_set_t dpu_set;
        DPU_ASSERT(dpu_alloc_ranks(nr_of_ranks, "nrThreadsPerRank=1", &dpu_set));
        DPU_ASSERT(dpu_load(dpu_set, binary.c_str(), NULL));
        load_from_dpu_set(dpu_set);
        if (nr_of_ranks != DPU_ALLOCATE_ALL) {
            assert(this->nr_of_ranks == nr_of_ranks);
        }
    }

    PIMInterface(dpu_set_t dpu_set): free_dpu_set_when_delete(true) { load_from_dpu_set(dpu_set); }

    virtual void Launch(bool async) = 0;

    void sync() { DPU_ASSERT(dpu_sync(dpu_set)); }

    template <typename F>
    void PrintLog(F filter) {
        DPU_FOREACH(dpu_set, dpu, each_dpu) {
            if (filter(each_dpu)) {
                printf("*** %d ***\n", each_dpu);
                DPU_ASSERT(dpu_log_read(dpu, stdout));
            }
        }
    }

    virtual void SendToPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name,
                           uint32_t symbol_offset, uint32_t length,
                           bool async) = 0;
    virtual void ReceiveFromPIM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name,
                                uint32_t symbol_offset, uint32_t length,
                                bool async) = 0;

    void SendToPIMByUPMEM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name,
                          uint32_t symbol_offset, uint32_t length,
                          bool async_transfer) {
        // Please make sure buffers don't overflow
        DPU_FOREACH(dpu_set, dpu, each_dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, buffers[each_dpu] + buffer_offset));
        }
        auto sync_setup = async_transfer ? DPU_XFER_ASYNC : DPU_XFER_DEFAULT;
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, symbol_name.c_str(),
                                 symbol_offset, length, sync_setup));
    }

    void ReceiveFromPIMByUPMEM(uint8_t** buffers, uint32_t buffer_offset, std::string symbol_name,
                               uint32_t symbol_offset, uint32_t length,
                               bool async_transfer) {
        // Please make sure buffers don't overflow
        DPU_FOREACH(dpu_set, dpu, each_dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, buffers[each_dpu] + buffer_offset));
        }
        auto sync_setup = async_transfer ? DPU_XFER_ASYNC : DPU_XFER_DEFAULT;
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU,
                                 symbol_name.c_str(), symbol_offset, length,
                                 sync_setup));
    }

    ~PIMInterface() {
        if (nr_of_ranks > 0) {
            if(free_dpu_set_when_delete) DPU_ASSERT(dpu_free(dpu_set));
            nr_of_ranks = nr_of_dpus = 0;
        }
    }

public:
    uint32_t GetNrOfRanks() const { return nr_of_ranks; }

    uint32_t GetNrOfDPUs() const { return nr_of_dpus; }

    void do_not_free_dpu_set_when_delete() { free_dpu_set_when_delete = false; }

protected:
    dpu_set_t dpu_set, dpu;
    uint32_t each_dpu;
    uint32_t nr_of_ranks, nr_of_dpus;

    bool free_dpu_set_when_delete;
};
