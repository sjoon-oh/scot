#pragma once

#include <stdint.h>
#include <time.h>

#include "./scot-def.hh"


#ifdef __cplusplus
extern "C" {
#endif

// Current SCOT_LOGALIGN_T : uint32_t
struct __attribute__((packed)) ScotAlignedLog {
    SCOT_LOGALIGN_T reserved[SCOT_LOGHEADER_RESERVED];
    SCOT_LOGALIGN_T aligned[SCOT_ALIGN_COUNTS];
};

struct __attribute__((packed)) ScotMessageHeader {
    SCOT_LOGALIGN_T hashv;
    uint16_t        buf_sz;
    uint8_t         inst;   // proposal number
    uint8_t         msg;
};

int scot_hash_cmp(void*, void*);

#ifdef __cplusplus
}
#endif

namespace scot {
    class ScotLog {
    private:
        uint32_t next_free;
        SCOT_LOGALIGN_T* log;

        uint8_t instn;

        uint32_t __get_next_aligned_idx(uint32_t, uint16_t);

    public:
        ScotLog(uint8_t*);
        ScotLog(SCOT_LOGALIGN_T*);
        ~ScotLog() = default;

        SCOT_LOGALIGN_T* write_local_log(struct ScotSlotEntry*);
        SCOT_LOGALIGN_T* poll_next_local_log(uint8_t);

        SCOT_LOGALIGN_T* get_base();
        SCOT_LOGALIGN_T* get_next_aligned_addr();

        uint8_t get_instn();
        uint32_t get_next_free();
    };
}

#define LOG_WRAPPER_INSTANCE(LOG)       (*(reinterpret_cast<struct ScotAlignedLog*>(LOG)))
