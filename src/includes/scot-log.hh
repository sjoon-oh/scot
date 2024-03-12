#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <stdint.h>
#include <time.h>

#include "./scot-def.hh"


#ifdef __cplusplus
extern "C" {
#endif

// Current SCOT_LOGALIGN_T : uint32_t
struct __attribute__((packed)) ScotAlignedLog {
    SCOT_LOGALIGN_T* aligned;
    size_t bound;
    SCOT_LOGALIGN_T reserved[SCOT_LOGHEADER_RESERVED];
};

enum {
    SCOT_RESERVED_IDX_FREE = 0,
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

    uint32_t next_aligned_idx(struct ScotAlignedLog*, uint16_t);

    SCOT_LOGALIGN_T* write_local_log(struct ScotAlignedLog*, struct ScotSlotEntry*, uint8_t);
    SCOT_LOGALIGN_T* poll_message_blocked(struct ScotAlignedLog*, uint8_t);
    SCOT_LOGALIGN_T* peek_message(struct ScotAlignedLog*, uint8_t);
}

#define LOG_LOOKALIKE(BASE)    (*(reinterpret_cast<struct ScotAlignedLog*>(BASE)))
