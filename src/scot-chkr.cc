
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


scot::ScotChecker::ScotChecker(SCOT_LOGALIGN_T* addr) 
    : ScotWriter(addr), msg_out("chkr"), wait_hashv(0) { 
    
    wait_lock.clear();
}

bool scot::ScotChecker::write_request(
    uint8_t* buf, uint16_t buf_sz, 
    uint8_t* key, uint16_t key_sz, 
    uint32_t hashv, uint32_t owner) {

    uint32_t index = slot.register_entry(
        {   
            .hashv      = hashv,
            .buffer     = buf,
            .buffer_sz  = buf_sz,
            .key        = key,
            .key_sz     = key_sz,
            .msg        = SCOT_MSGTYPE_WAIT
        }
    );

    // HT insert
    struct ScotSlotEntry* curr = slot.get_slot_entry(index);

    __START_WRITE__ {

        SCOT_LOGALIGN_T* header = log.write_local_log(curr); // Write to the local log here.        
        size_t log_sz = sizeof(struct ScotMessageHeader) + 
            static_cast<size_t>(curr->buffer_sz) + sizeof(uint8_t);

        size_t offset = uintptr_t(header) - uintptr_t(log.get_base());
        SCOT_LOGALIGN_T* remote_target_addr;

        bool ret = 0;

#ifdef __DEBUG__
        struct ScotMessageHeader* local_header = 
            reinterpret_cast<struct ScotMessageHeader*>(header);

        uint32_t unbind_hashv   = local_header->hashv;
        uint16_t unbind_bufsz   = local_header->buf_sz;
        uint8_t unbind_inst     = local_header->inst;
        uint8_t unbind_msg      = local_header->msg;

        SCOT_LOG_FINEGRAINED_T* local_payload = 
            (SCOT_LOG_FINEGRAINED_T*)(header) + (sizeof(struct ScotMessageHeader));

        __SCOT_INFO__(
            msg_out, 
            "→→ RDMA message local header: \n-- Report --\nhashv({}), buf_sz({}), inst({}), msg({}), total {} bytes\n----", 
            unbind_hashv, unbind_bufsz, unbind_inst, unbind_msg, log_sz
        );
#endif

        wait_hashv = hashv;

        // 
        // Send WR first for all quorums
        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
            
            remote_target_addr = 
                reinterpret_cast<SCOT_LOGALIGN_T*>(
                        uintptr_t(ctx.remote.rcvr_mr->addr) + offset);

            if (owner == ctx.nid) {
                reinterpret_cast<struct ScotMessageHeader*>(header)->msg = 
                    SCOT_MSGTYPE_WAIT;

                ret = hartebeest_rdma_post_single_fast(
                    ctx.local.chkr_qp,          // Local QP (Checker)
                    header,                     // Local starting point of a RDMA message
                    remote_target_addr,         // To where at remote?
                    log_sz,                     // Total message size
                    IBV_WR_RDMA_WRITE,
                    ctx.local.chkr_mr->lkey,    // Local MR acces key
                    ctx.remote.rcvr_mr->rkey,   // Remote MR access key
                    ctx.nid);
            }
            else {
                reinterpret_cast<struct ScotMessageHeader*>(header)->msg = 
                    SCOT_MSGTYPE_HDRONLY;

                ret = hartebeest_rdma_post_single_fast(
                    ctx.local.chkr_qp,          // Local QP (Checker)
                    header,                     // Local starting point of a RDMA message
                    remote_target_addr,         // To where at remote?
                    sizeof(struct ScotMessageHeader),
                                                // Total message size
                    IBV_WR_RDMA_WRITE,
                    ctx.local.chkr_mr->lkey,    // Local MR acces key
                    ctx.remote.rcvr_mr->rkey,   // Remote MR access key
                    ctx.nid);
            }

            ret = hartebeest_rdma_send_poll(ctx.local.chkr_qp);

#ifdef __DEBUG__
            assert(ret != false);
#endif
        }


#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ write_request end: {}", index);
#endif

        __WAIT_FOR_PROPACK__

    } __END_WRITE__

    wait_hashv = 0;

    if (slot.mark_entry_finished(index, curr) == SCOT_SLOT_RESET) { // Reset comes here.
#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ Slot/hasht reset triggered");
#endif
    }
    
    return 0;
}


void scot::ScotChecker::release_wait(uint32_t hashv) {
    
    if (wait_hashv == hashv) {
        __RELEASE_AT_PROPACK__

#ifdef __DEBUG__
    __SCOT_INFO__(msg_out, "→→ Checker released, for {}", wait_hashv);
#endif
    }
}