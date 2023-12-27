
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"

void scot::ScotReplicator::ScotReplicator::__ht_try_insert(struct ScotSlotEntry* entry) {
    bool is_samekey, is_success;
    struct ScotSlotEntry *prev, *next;

    do {
        is_samekey = hasht.search(entry->hashv, entry, &prev, &next);

        if (!is_samekey) is_success = hasht.insert(entry, prev, next);
        else is_success = hasht.replace(entry, prev, next);

    } while(!is_success);
}

struct ScotSlotEntry* scot::ScotReplicator::ScotReplicator::__ht_get_latest_entry(struct ScotSlotEntry* curr) {

    uint32_t hashv = curr->hashv;

    // Search for the latest slot
    while (IS_MARKED_AS_DELETED(curr->next) && (hashv == curr->hashv)) {
        
        curr->ready = 0;
        curr = GET_UNMARKED_REFERENCE(curr->next); // Move to next
    }

    assert(hashv == curr->hashv);

    return curr;
}


scot::ScotReplicator::ScotReplicator(SCOT_LOGALIGN_T* addr) 
    : ScotWriter(addr), hasht(SCOT_HT_SIZE, scot_hash_cmp), msg_out("rpli") { 
}

bool scot::ScotReplicator::write_request(
    uint8_t* buf, uint16_t buf_sz, 
    uint8_t* key, uint16_t key_sz, 
    uint32_t hashv, uint8_t msg) {

    uint32_t index = slot.register_entry(
        {   
            .hashv      = hashv,
            .buffer     = buf,
            .buffer_sz  = buf_sz,
            .key        = key,
            .key_sz     = key_sz,
            .msg        = msg
        }
    );

    // HT insert
    struct ScotSlotEntry* curr = slot.get_slot_entry(index);
    struct ScotSlotEntry* latest = curr;

    __ht_try_insert(curr);
    __START_WRITE__ {

        latest = __ht_get_latest_entry(curr);

        SCOT_LOGALIGN_T* header = log.write_local_log(latest); // Write to the local log here.        
        size_t log_sz = sizeof(struct ScotMessageHeader) + 
            static_cast<size_t>(latest->buffer_sz) + sizeof(uint8_t);

        size_t offset = uintptr_t(header) - uintptr_t(log.get_base());
        SCOT_LOGALIGN_T* remote_target_addr;

        bool ret = 0;

#ifdef _ON_DEBUG_X
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

        // 
        // Send WR first for all quorums
        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
            
            remote_target_addr = 
                reinterpret_cast<SCOT_LOGALIGN_T*>(
                        uintptr_t(ctx.remote.rply_mr->addr) + offset);

            ret = hartebeest_rdma_post_single_fast(
                ctx.local.rpli_qp,          // Local QP
                header,                     // Local starting point of a RDMA message
                remote_target_addr,         // To where at remote?
                log_sz,                     // Total message size
                IBV_WR_RDMA_WRITE,
                ctx.local.rpli_mr->lkey,    // Local MR acces key
                ctx.remote.rply_mr->rkey,   // Remote MR access key
                ctx.nid);

#ifdef _ON_DEBUG_
            assert(ret != false);
#endif
        }

        //
        // Wait for else.
        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
            ret = hartebeest_rdma_send_poll(ctx.local.rpli_qp);

#ifdef _ON_DEBUG_
            if (!ret) {
                __SCOT_INFO__(msg_out, "→→ Polling error.");
                assert(0);
            }
#endif
        }

#ifdef _ON_DEBUG_X
        __SCOT_INFO__(msg_out, "→→ write_request end: {}", index);
#endif

    } __END_WRITE__

    if (slot.mark_entry_finished(index, latest, &hasht) == SCOT_SLOT_RESET) { // Reset comes here.
#ifdef _ON_DEBUG_X
        __SCOT_INFO__(msg_out, "→→ Slot/hasht reset triggered");
#endif
    }
    
    return 0;
}