#include <vector>
#include <string>
#include <memory>

#include <cstring>

#include <assert.h>
#include <stdlib.h>

#include "./includes/scot-slot.hh"
#include "./includes/scot-log.hh"

int scot_hash_cmp(void* a, void* b) {

    ScotSlotEntry* target_a = reinterpret_cast<ScotSlotEntry*>(a);
    ScotSlotEntry* target_b = reinterpret_cast<ScotSlotEntry*>(b);

    if (target_a->hashv == target_b->hashv) 
        return 0;

    else if (target_a->hashv > target_b->hashv)
        return 1;

    else return -1;
}


scot::ScotLog::ScotLog(uint8_t* mr_addr) : next_free(0) {

    log = reinterpret_cast<struct ScotAlignedLog*>(mr_addr);
    std::memset(log, 0, sizeof(struct ScotAlignedLog));
}


scot::ScotLog::ScotLog(struct ScotAlignedLog* mr_addr) : next_free(0) {
    
    log = mr_addr;
    std::memset(log, 0, sizeof(struct ScotAlignedLog));
}


uint32_t scot::ScotLog::next_aligned(uint32_t cur_align, uint16_t buf_sz) {

    if (buf_sz % sizeof(SCOT_LOGALIGN_T) != 0)
        buf_sz += sizeof(SCOT_LOGALIGN_T);
    
    uint32_t take = buf_sz / sizeof(SCOT_LOGALIGN_T);
    uint32_t next_align = cur_align + take;

    if (next_align > (SCOT_ALIGN_COUNTS - 1))
        return 0;

    return next_align;
}






