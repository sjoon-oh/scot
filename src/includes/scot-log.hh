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
        struct ScotAlignedLog* log;

        uint8_t instn;

    public:
        ScotLog(uint8_t*);
        ScotLog(struct ScotAlignedLog*);
        ~ScotLog() = default;

        uint32_t next_aligned(uint32_t, uint16_t);
        SCOT_LOGALIGN_T* write_local_log(struct ScotSlotEntry*);
    };


}






