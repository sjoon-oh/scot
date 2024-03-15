/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <vector>
#include <string>
#include <memory>

#include <iostream>

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


uint32_t scot::next_aligned_idx(struct ScotAlignedLog* log, uint16_t buf_sz) {

    // If not aligned, give extra space.
    // buf_sz should contain physical size, including header, payload, canary etc.
    uint32_t cur_idx = log->reserved[SCOT_RESERVED_IDX_FREE];

    if (buf_sz % sizeof(SCOT_LOGALIGN_T) != 0)
        buf_sz += sizeof(SCOT_LOGALIGN_T);
    
    uint32_t take = buf_sz / sizeof(SCOT_LOGALIGN_T);
    uint32_t next_idx = cur_idx + take;

    if (next_idx > log->bound)
        return 0;

    return next_idx;
}


SCOT_LOGALIGN_T* scot::write_local_log(struct ScotAlignedLog* log, struct ScotSlotEntry* request, uint8_t instn) {

    // Make message and write to the local log
    uint32_t cur_idx = log->reserved[SCOT_RESERVED_IDX_FREE];

    SCOT_LOG_FINEGRAINED_T* pos = 
        FINEPTR_LOOKALIKE(&(log->aligned[cur_idx]));

    //
    // 1. Record the message header
    struct ScotMessageHeader* header = reinterpret_cast<struct ScotMessageHeader*>(pos);
    header->hashv   = request->hashv;
    header->buf_sz  = request->buffer_sz;
    header->inst    = instn;
    header->msg     = request->msg;

    instn++;
    cur_idx += 2;   //sizeof(struct ScotMessageHeader) in index

    // 
    // 2. Record the payload
    pos = reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(&(log->aligned[cur_idx]));

    std::memcpy(pos, request->buffer, request->buffer_sz);
    pos += uintptr_t(request->buffer_sz);

    //
    // 3. Record the Canary
    uint8_t canary = SCOT_LOGENTRY_CANARY;
    std::memcpy(pos, &canary, sizeof(canary));

    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;
    cur_idx = next_aligned_idx(log, (request->buffer_sz + sizeof(canary)));

    // Update index
    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(header);
}


SCOT_LOGALIGN_T* scot::poll_message_blocked(struct ScotAlignedLog* log, uint8_t msg_detect) {

    uint32_t cur_idx = log->reserved[SCOT_RESERVED_IDX_FREE];

    SCOT_LOG_FINEGRAINED_T* log_pos = 
        FINEPTR_LOOKALIKE(&(log->aligned[cur_idx]));
        
    SCOT_LOG_FINEGRAINED_T* pld_pos = log_pos;
    SCOT_LOG_FINEGRAINED_T* cnry_pos = nullptr;

    struct ScotMessageHeader* rcv_header = 
        reinterpret_cast<struct ScotMessageHeader*>(log_pos);

    // Wait until a message is written from the remote.
    // 1. Wait for message detect.
    uint8_t detect_type = 0;
    while ((detect_type = (rcv_header->msg & msg_detect)) == 0)
        detect_type = 0;

    cur_idx += 2;
    pld_pos += uintptr_t(sizeof(struct ScotMessageHeader));

#ifdef __DEBUG__

    std::string mtype("");
    if (rcv_header->msg & SCOT_MSGTYPE_ACK)
        mtype += "|ACK|";
    if (rcv_header->msg & SCOT_MSGTYPE_PURE)
        mtype += "|PURE|";
    if (rcv_header->msg & SCOT_MSGTYPE_WAIT)
        mtype += "|WAIT|";
    if (rcv_header->msg & SCOT_MSGTYPE_HDRONLY)
        mtype += "|HDR|";

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

    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;
    cur_idx = next_aligned_idx(log, (rcv_header->buf_sz + sizeof(uint8_t)));

    // Update index
    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(rcv_header);
}


SCOT_LOGALIGN_T* scot::peek_message(struct ScotAlignedLog* log, uint8_t msg_detect) {

    uint32_t cur_idx = log->reserved[SCOT_RESERVED_IDX_FREE];

    SCOT_LOG_FINEGRAINED_T* log_pos = 
        FINEPTR_LOOKALIKE(uintptr_t(log->aligned) + (cur_idx * sizeof(SCOT_LOGALIGN_T)));

    // std::cout << "PEEK: Current index " << cur_idx << " addr: " << std::hex << log_pos << std::endl;

    SCOT_LOG_FINEGRAINED_T* pld_pos = log_pos;
    SCOT_LOG_FINEGRAINED_T* cnry_pos = nullptr;

    struct ScotMessageHeader* rcv_header = 
        reinterpret_cast<struct ScotMessageHeader*>(log_pos);

    assert(rcv_header != nullptr);

    // Wait until a message is written from the remote.
    // 1. Wait for message detect.
    uint8_t detect_type = 0;
    if ((detect_type = (rcv_header->msg & msg_detect)) == 0) {
        return nullptr;
    }

    cur_idx += 2;
    pld_pos += uintptr_t(sizeof(struct ScotMessageHeader));

    // rcv_header->msg = 0;

    // 3. Wait for canary (Validation).
    cnry_pos = pld_pos + uintptr_t(rcv_header->buf_sz);
    if (rcv_header->msg != SCOT_MSGTYPE_HDRONLY) {
        if ((*(reinterpret_cast<uint8_t*>(cnry_pos))) != SCOT_LOGENTRY_CANARY) {
            return nullptr;
        }
        else
            *(reinterpret_cast<uint8_t*>(cnry_pos)) = 0; // Reset canary value.
    }

#ifdef __DEBUG__

    std::string mtype("");
    if (rcv_header->msg & SCOT_MSGTYPE_ACK)
        mtype += "|ACK|";
    if (rcv_header->msg & SCOT_MSGTYPE_PURE)
        mtype += "|PURE|";
    if (rcv_header->msg & SCOT_MSGTYPE_WAIT)
        mtype += "|WAIT|";
    if (rcv_header->msg & SCOT_MSGTYPE_HDRONLY)
        mtype += "|HDR|";

    printf("Detected {hashv: %ld, bufsz: %ld, msg: %s, payload: %s}.\n", 
        rcv_header->hashv,
        rcv_header->buf_sz,
        mtype.c_str(),
        (char*)pld_pos
        );
#endif

    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;
    cur_idx = next_aligned_idx(log, (rcv_header->buf_sz + sizeof(uint8_t)));

    // Update index
    log->reserved[SCOT_RESERVED_IDX_FREE] = cur_idx;

    return reinterpret_cast<SCOT_LOGALIGN_T*>(rcv_header);
}