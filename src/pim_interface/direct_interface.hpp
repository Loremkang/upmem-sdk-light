#pragma once

#include <immintrin.h>
#include <x86intrin.h>

#include <cinttypes>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <string>

extern "C" {
#include <dpu.h>
#include <dpu_internals.h>
#include <dpu_management.h>
#include <dpu_program.h>
#include <dpu_rank.h>
#include <dpu_region_address_translation.h>
#include <dpu_target_macros.h>
#include <dpu_types.h>
#include <ufi_config.h>

#include "backends/hw/src/commons/dpu_region_constants.h"
#include "backends/hw/src/rank/hw_dpu_sysfs.h"
#include "backends/ufi/include/ufi/ufi.h"

typedef struct _fpga_allocation_parameters_t {
    bool activate_ila;
    bool activate_filtering_ila;
    bool activate_mram_bypass;
    bool activate_mram_refresh_emulation;
    unsigned int mram_refresh_emulation_period;
    char *report_path;
    bool cycle_accurate;
} fpga_allocation_parameters_t;

typedef struct _hw_dpu_rank_allocation_parameters_t {
    struct dpu_rank_fs rank_fs;
    struct dpu_region_address_translation translate;
    uint64_t region_size;
    uint8_t mode, dpu_chip_id, backend_id;
    uint8_t channel_id;
    uint8_t *ptr_region;
    bool bypass_module_compatibility;
    /* Backends specific */
    fpga_allocation_parameters_t fpga;
} *hw_dpu_rank_allocation_parameters_t;
}

const uint32_t MAX_NR_RANKS = 40;
const uint32_t DPU_PER_RANK = 64;
const uint64_t MRAM_SIZE = (64 << 20);

class DirectPIMInterface {
   public:
    DirectPIMInterface(uint32_t nr_of_ranks, std::string binary) {
        dpu_set_t dpu_set;
        DPU_ASSERT(
            dpu_alloc_ranks(nr_of_ranks, "nrThreadsPerRank=1", &dpu_set));
        DPU_ASSERT(dpu_load(dpu_set, binary.c_str(), NULL));
        load_from_dpu_set(dpu_set);
        if (nr_of_ranks != DPU_ALLOCATE_ALL) {
            assert(this->nr_of_ranks == nr_of_ranks);
        }
    }

    DirectPIMInterface(dpu_set_t dpu_set) { load_from_dpu_set(dpu_set); }

    void load_from_dpu_set(dpu_set_t dpu_set) {
        this->dpu_set = dpu_set;
        DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &this->nr_of_dpus));
        DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &this->nr_of_ranks));
        std::printf("Allocated %d DPU(s)\n", this->nr_of_dpus);
        std::printf("Allocated %d Ranks(s)\n", this->nr_of_ranks);
        assert(this->nr_of_dpus <= nr_of_ranks * DPU_PER_RANK);

        ranks = new dpu_rank_t *[nr_of_ranks];
        params = new hw_dpu_rank_allocation_parameters_t[nr_of_ranks];
        base_addrs = new uint8_t *[nr_of_ranks];
        program = nullptr;
        for (uint32_t i = 0; i < nr_of_ranks; i++) {
            ranks[i] = dpu_set.list.ranks[i];
            params[i] =
                ((hw_dpu_rank_allocation_parameters_t)(ranks[i]
                                                           ->description
                                                           ->_internals.data));
            base_addrs[i] = params[i]->ptr_region;
        }
        // find program pointer
        DPU_FOREACH(dpu_set, dpu, each_dpu) {
            assert(dpu.kind == DPU_SET_DPU);
            dpu_t *dpuptr = dpu.dpu;
            if (!dpu_is_enabled(dpuptr)) {
                continue;
            }
            dpu_program_t *this_program = dpu_get_program(dpuptr);
            if (program == nullptr) {
                program = this_program;
            }
            assert(program == nullptr || program == this_program);
        }
    }

    ~DirectPIMInterface() {
        if (nr_of_ranks > 0) {
            DPU_ASSERT(dpu_free(dpu_set));
            nr_of_ranks = nr_of_dpus = 0;
        }
    }

    inline bool aligned(uint64_t offset, uint64_t factor) {
        return (offset % factor == 0);
    }

    inline uint64_t GetCorrectOffsetMRAM(uint64_t address_offset,
                                         uint32_t dpu_id) {
        auto FastPath = [](uint64_t address_offset, uint32_t dpu_id) {
            uint64_t mask_move_7 =
                (~((1 << 22) - 1)) + (1 << 13);              // 31..22, 13
            uint64_t mask_move_6 = ((1 << 22) - (1 << 15));  // 21..15
            uint64_t mask_move_14 = (1 << 14);               // 14
            uint64_t mask_move_4 = (1 << 13) - 1;            // 12 .. 0
            return ((address_offset & mask_move_7) << 7) |
                   ((address_offset & mask_move_6) << 6) |
                   ((address_offset & mask_move_14) << 14) |
                   ((address_offset & mask_move_4) << 4) | (dpu_id << 18);
        };

        // not used
        auto OraclePath = [](uint64_t address_offset, uint32_t dpu_id) {
            // uint64_t fastoffset = get_correct_offset_fast(address_offset,
            // dpu_id);
            uint64_t offset = 0;
            // 1 : address_offset < 64MB
            offset += (512ll << 20) * (address_offset >> 22);
            address_offset &= (1ll << 22) - 1;
            // 2 : address_offset < 4MB
            if (address_offset & (16 << 10)) {
                offset += (256ll << 20);
            }
            offset += (2ll << 20) * (address_offset / (32 << 10));
            address_offset %= (16 << 10);
            // 3 : address_offset < 16K
            if (address_offset & (8 << 10)) {
                offset += (1ll << 20);
            }
            address_offset %= (8 << 10);
            offset += address_offset * 16;
            // 4 : address_offset < 8K
            offset += (dpu_id & 3) * (256 << 10);
            // 5
            if (dpu_id >= 4) {
                offset += 64;
            }
            return offset;
        };
        (void)OraclePath;

        // uint64_t v1 = FastPath(address_offset, dpu_id);
        // uint64_t v2 = OraclePath(address_offset, dpu_id);
        // assert(v1 == v2);
        // return v1;

        return FastPath(address_offset, dpu_id);
    }

    void ReceiveFromRankMRAM(uint8_t **buffers, uint32_t symbol_offset,
                             uint8_t *ptr_dest, uint32_t length) {
        assert(aligned(symbol_offset, sizeof(uint64_t)));
        assert(aligned(length, sizeof(uint64_t)));
        assert((uint64_t)symbol_offset + length <= MRAM_SIZE);

        uint64_t cache_line[8], cache_line_interleave[8];

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                // 8 shards of DPUs
                uint64_t offset =
                    GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);
                __builtin_ia32_clflushopt((void *)(ptr_dest + offset));
                offset += 0x40;
                __builtin_ia32_clflushopt((void *)(ptr_dest + offset));
            }
        }
        __builtin_ia32_mfence();

        auto LoadData = [](uint64_t *cache_line, uint8_t *ptr_dest) {
            cache_line[0] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    0 * sizeof(uint64_t)));
            cache_line[1] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    1 * sizeof(uint64_t)));
            cache_line[2] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    2 * sizeof(uint64_t)));
            cache_line[3] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    3 * sizeof(uint64_t)));
            cache_line[4] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    4 * sizeof(uint64_t)));
            cache_line[5] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    5 * sizeof(uint64_t)));
            cache_line[6] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    6 * sizeof(uint64_t)));
            cache_line[7] = *((volatile uint64_t *)((uint8_t *)ptr_dest +
                                                    7 * sizeof(uint64_t)));
        };

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                if ((i % 8 == 0) && (i + 8 < length / sizeof(uint64_t))) {
                    for (int j = 0; j < 16; j++) {
                        __builtin_prefetch(
                            ((uint64_t *)buffers[j * 4 + dpu_id]) + i + 8);
                    }
                }
                uint64_t offset =
                    GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);
                __builtin_prefetch(ptr_dest + offset + 0x40 * 6);
                __builtin_prefetch(ptr_dest + offset + 0x40 * 7);

                LoadData(cache_line, ptr_dest + offset);
                byte_interleave_avx2(cache_line, cache_line_interleave);
                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id] == nullptr) {
                        continue;
                    }
                    *(((uint64_t *)buffers[j * 8 + dpu_id]) + i) =
                        cache_line_interleave[j];
                }

                offset += 0x40;
                LoadData(cache_line, ptr_dest + offset);
                byte_interleave_avx2(cache_line, cache_line_interleave);
                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id + 4] == nullptr) {
                        continue;
                    }
                    *(((uint64_t *)buffers[j * 8 + dpu_id + 4]) + i) =
                        cache_line_interleave[j];
                }
            }
        }
    }

    void SendToRankMRAM(uint8_t **buffers, uint32_t symbol_offset,
                        uint8_t *ptr_dest, uint32_t length) {
        assert(aligned(symbol_offset, sizeof(uint64_t)));
        assert(aligned(length, sizeof(uint64_t)));
        assert((uint64_t)symbol_offset + length <= MRAM_SIZE);

        uint64_t cache_line[8];

        for (uint32_t dpu_id = 0; dpu_id < 4; ++dpu_id) {
            for (uint32_t i = 0; i < length / sizeof(uint64_t); ++i) {
                if ((i % 8 == 0) && (i + 8 < length / sizeof(uint64_t))) {
                    for (int j = 0; j < 16; j++) {
                        __builtin_prefetch(
                            ((uint64_t *)buffers[j * 4 + dpu_id]) + i + 8);
                    }
                }
                uint64_t offset =
                    GetCorrectOffsetMRAM(symbol_offset + (i * 8), dpu_id);

                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id] == nullptr) {
                        continue;
                    }
                    cache_line[j] =
                        *(((uint64_t *)buffers[j * 8 + dpu_id]) + i);
                }
                byte_interleave_avx2(cache_line, (uint64_t *)(ptr_dest + offset));

                offset += 0x40;
                for (int j = 0; j < 8; j++) {
                    if (buffers[j * 8 + dpu_id + 4] == nullptr) {
                        continue;
                    }
                    cache_line[j] =
                        *(((uint64_t *)buffers[j * 8 + dpu_id + 4]) + i);
                }
                byte_interleave_avx2(cache_line, (uint64_t *)(ptr_dest + offset));
            }
        }

        __builtin_ia32_mfence();
    }

    bool DirectAvailable(bool async_transfer) {
        // Only suport synchronous transfer
        if (async_transfer) {
            return false;
        }

        for (uint32_t i = 0; i < nr_of_ranks; i++) {
            if (params[i]->mode != DPU_REGION_MODE_PERF) {
                return false;
            }
        }

        return true;
    }

    // Find symbol address offset
    uint32_t GetSymbolOffset(std::string symbol_name) {
        dpu_symbol_t symbol;
        DPU_ASSERT(dpu_get_symbol(program, symbol_name.c_str(), &symbol));
        return symbol.address;
    }

   public:
    // not modifying Launch currently because the default "error handling" seems
    // to be useful.
    void Launch(bool async) {
        auto async_parameter = async ? DPU_ASYNCHRONOUS : DPU_SYNCHRONOUS;
        DPU_ASSERT(dpu_launch(dpu_set, async_parameter));
    }

    void ReceiveFromMRAM(uint8_t **buffers, uint32_t symbol_base_offset,
                         uint32_t symbol_offset, uint32_t length,
                         bool async_transfer) {
        assert(DirectAvailable(async_transfer));
        assert(symbol_base_offset & MRAM_ADDRESS_SPACE);
        symbol_offset += symbol_base_offset ^ MRAM_ADDRESS_SPACE;
        auto ReceiveFromIthRank = [&](size_t i) {
            DPU_ASSERT(dpu_switch_mux_for_rank(ranks[i], true));
            ReceiveFromRankMRAM(&buffers[i * MAX_NR_DPUS_PER_RANK],
                                symbol_offset, base_addrs[i], length);
        };

        for (size_t i = 0; i < nr_of_ranks; i++) {
            ReceiveFromIthRank(i);
        }
    }

    void ReceiveFromPIM(uint8_t **buffers, std::string symbol_name,
                        uint32_t symbol_offset, uint32_t length,
                        bool async_transfer) {
        // Please make sure buffers don't overflow
        assert(DirectAvailable(async_transfer));

        uint32_t symbol_base_offset = GetSymbolOffset(symbol_name);

        // Skip disabled PIM modules
        uint8_t *buffers_alligned[MAX_NR_RANKS * MAX_NR_DPUS_PER_RANK];
        {
            uint32_t offset = 0;
            for (uint32_t i = 0; i < nr_of_ranks; i++) {
                for (int j = 0; j < MAX_NR_DPUS_PER_RANK; j++) {
                    if (ranks[i]->dpus[j].enabled) {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            buffers[offset++];
                    } else {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            nullptr;
                    }
                }
            }
            assert(offset == nr_of_dpus);
        }

        assert(symbol_base_offset & MRAM_ADDRESS_SPACE);
        // Only support heap pointer at present
        //assert(symbol_name == DPU_MRAM_HEAP_POINTER_NAME);
        ReceiveFromMRAM(buffers_alligned, symbol_base_offset, symbol_offset,
                        length, async_transfer);
    }

    void SendToPIM(uint8_t **buffers, std::string symbol_name,
                   uint32_t symbol_offset, uint32_t length,
                   bool async_transfer) {
        // Please make sure buffers don't overflow
        assert(DirectAvailable(async_transfer));

        assert(GetSymbolOffset(symbol_name) & MRAM_ADDRESS_SPACE);
        symbol_offset += GetSymbolOffset(symbol_name) ^ MRAM_ADDRESS_SPACE;

        // Skip disabled PIM modules
        uint8_t *buffers_alligned[MAX_NR_RANKS * MAX_NR_DPUS_PER_RANK];
        {
            uint32_t offset = 0;
            for (uint32_t i = 0; i < nr_of_ranks; i++) {
                for (int j = 0; j < MAX_NR_DPUS_PER_RANK; j++) {
                    if (ranks[i]->dpus[j].enabled) {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            buffers[offset++];
                    } else {
                        buffers_alligned[i * MAX_NR_DPUS_PER_RANK + j] =
                            nullptr;
                    }
                }
            }
            assert(offset == nr_of_dpus);
        }

        auto SendToIthRank = [&](size_t i) {
            DPU_ASSERT(dpu_switch_mux_for_rank(ranks[i], true));
            SendToRankMRAM(&buffers_alligned[i * MAX_NR_DPUS_PER_RANK],
                           symbol_offset, base_addrs[i], length);
        };

        for (uint32_t i = 0; i < nr_of_ranks; i++) {
            SendToIthRank(i);
        }
    }

   private:
    void byte_interleave_avx2(uint64_t *input, uint64_t *output) {
        __m256i tm = _mm256_set_epi8(
            15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0,

            15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
        char *src1 = (char *)input, *dst1 = (char *)output;

        __m256i vindex = _mm256_setr_epi32(0, 8, 16, 24, 32, 40, 48, 56);
        __m256i perm = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

        __m256i load0 = _mm256_i32gather_epi32((int *)src1, vindex, 1);
        __m256i load1 = _mm256_i32gather_epi32((int *)(src1 + 4), vindex, 1);

        __m256i transpose0 = _mm256_shuffle_epi8(load0, tm);
        __m256i transpose1 = _mm256_shuffle_epi8(load1, tm);

        __m256i final0 = _mm256_permutevar8x32_epi32(transpose0, perm);
        __m256i final1 = _mm256_permutevar8x32_epi32(transpose1, perm);

        _mm256_storeu_si256((__m256i *)&dst1[0], final0);
        _mm256_storeu_si256((__m256i *)&dst1[32], final1);
    }

   public:
    uint32_t GetNrOfRanks() const { return nr_of_ranks; }
    uint32_t GetNrOfDPUs() const { return nr_of_dpus; }

   protected:
    dpu_set_t dpu_set, dpu;
    uint32_t each_dpu;
    uint32_t nr_of_ranks, nr_of_dpus;

    const int MRAM_ADDRESS_SPACE = 0x8000000;
    dpu_rank_t **ranks;
    hw_dpu_rank_allocation_parameters_t *params;
    uint8_t **base_addrs;
    dpu_program_t *program;
    // map<std::string, uint32_t> offset_list;
};