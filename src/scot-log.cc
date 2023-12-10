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


scot::ScotLog::ScotLog(uint8_t* mr_addr) : next_free(0), instn(0) {

    log = reinterpret_cast<struct ScotAlignedLog*>(mr_addr);
    std::memset(log, 0, sizeof(struct ScotAlignedLog));
}


scot::ScotLog::ScotLog(struct ScotAlignedLog* mr_addr) : next_free(0), instn(0) {
    
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


SCOT_LOGALIGN_T* scot::ScotLog::write_local_log(struct ScotSlotEntry* request) {

    // Make message and write to the local log
    uint32_t cur_align_index = next_free;
    SCOT_LOGALIGN_T* log_pos = &(log->aligned[cur_align_index]);

    //
    // 1. Record the message header
    struct ScotMessageHeader* header = reinterpret_cast<struct ScotMessageHeader*>(log_pos);
    header->hashv   = request->hashv;
    header->buf_sz  = request->buffer_sz;
    header->inst    = instn;
    header->msg     = request->msg;

    instn++;
    cur_align_index += 2; //sizeof(struct ScotMessageHeader) in index

    // 
    // 2. Record the payload
    log_pos = &(log->aligned[cur_align_index]);
    std::memcpy(log_pos, request->buffer, request->buffer_sz);

    log_pos += (request->buffer_sz);

    //
    // 3. Record the Canary
    uint8_t canary = SCOT_LOGENTRY_CANARY;
    std::memcpy(log_pos, &canary, sizeof(canary));

    cur_align_index = next_aligned(cur_align_index, (request->buffer_sz + sizeof(canary)));

    // Update index
    next_free = cur_align_index;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(header);
}






