#include <vector>
#include <string>
#include <memory>

#include <cstring>
#include <cstdio>

#include <assert.h>
#include <stdlib.h>

#include "./includes/scot-def.hh"

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

    log = reinterpret_cast<SCOT_LOGALIGN_T*>(mr_addr);
    std::memset(log, 0, sizeof(struct ScotAlignedLog));
}


scot::ScotLog::ScotLog(SCOT_LOGALIGN_T* mr_addr) : next_free(0), instn(0) {
    
    log = mr_addr;
    std::memset(log, 0, sizeof(struct ScotAlignedLog));
}


uint32_t scot::ScotLog::__get_next_aligned_idx(uint32_t cur_align, uint16_t buf_sz) {

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

    SCOT_LOG_FINEGRAINED_T* log_pos = 
        reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(
            &(LOG_WRAPPER_INSTANCE(log).aligned[cur_align_index])
        );

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


    log_pos = reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(
            &(LOG_WRAPPER_INSTANCE(log).aligned[cur_align_index])
        );
    std::memcpy(log_pos, request->buffer, request->buffer_sz);

    log_pos += uintptr_t(request->buffer_sz);

    //
    // 3. Record the Canary
    uint8_t canary = SCOT_LOGENTRY_CANARY;
    std::memcpy(log_pos, &canary, sizeof(canary));

    cur_align_index = __get_next_aligned_idx(cur_align_index, (request->buffer_sz + sizeof(canary)));

    // Update index
    next_free = cur_align_index;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(header);
}


SCOT_LOGALIGN_T* scot::ScotLog::poll_next_local_log(uint8_t msg_detect) {

    uint32_t cur_align_index = next_free;

    SCOT_LOG_FINEGRAINED_T* log_pos = 
        reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(
            &(LOG_WRAPPER_INSTANCE(log).aligned[cur_align_index])
        );
        
    SCOT_LOG_FINEGRAINED_T* pld_pos = log_pos;
    SCOT_LOG_FINEGRAINED_T* cnry_pos = nullptr;

    struct ScotMessageHeader* rcv_header = 
        reinterpret_cast<struct ScotMessageHeader*>(log_pos);

    // Wait until a message is written from the remote.
    // 1. Wait for message detect.
    uint8_t detect_type = 0;
    while ((detect_type = (rcv_header->msg & msg_detect)) == 0)
        detect_type = 0;

    cur_align_index += 2;
    pld_pos += uintptr_t(sizeof(struct ScotMessageHeader));

#ifdef __DEBUG__

    std::string mtype;
    switch (rcv_header->msg) {
        case SCOT_MSGTYPE_ACK: mtype = "ACK"; break;
        case SCOT_MSGTYPE_PURE: mtype = "PURE"; break;
        case SCOT_MSGTYPE_WAIT: mtype = "WAIT"; break;
        case SCOT_MSGTYPE_HDRONLY: mtype = "HDR"; break;
    }

    printf("Detected {hashv: %ld, bufsz: %ld, msg: %s, payload: %s}.\n", 
        rcv_header->hashv,
        rcv_header->buf_sz,
        mtype.c_str(),
        (char*)pld_pos
        );
    
#endif

    // rcv_header->msg = 0;

    // 3. Wait for canary (Validation).
    cnry_pos = pld_pos + uintptr_t(rcv_header->buf_sz);
    if (rcv_header->msg != SCOT_MSGTYPE_HDRONLY) {
        while ((*(reinterpret_cast<uint8_t*>(cnry_pos))) != SCOT_LOGENTRY_CANARY) {

        }
        *(reinterpret_cast<uint8_t*>(cnry_pos)) = 0; // Reset canary value.
    }

    cur_align_index = __get_next_aligned_idx(cur_align_index, 
        (rcv_header->buf_sz + sizeof(uint8_t)));
    next_free = cur_align_index;

    instn++;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(rcv_header);
}


SCOT_LOGALIGN_T* scot::ScotLog::get_base() {
    return log;
}


SCOT_LOGALIGN_T* scot::ScotLog::get_next_aligned_addr() {
    uint32_t cur_align_index = next_free;
    return &(LOG_WRAPPER_INSTANCE(log).aligned[cur_align_index]);
}


uint8_t scot::ScotLog::get_instn() {
    return instn;
}


uint32_t scot::ScotLog::get_next_free() {
    return next_free;
}
