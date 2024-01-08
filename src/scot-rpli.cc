
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

    /*
    // Make piggyback commit.
    uint8_t new_msg = msg;
    if (msg | SCOT_MSGTYPE_PURE)
        new_msg |= SCOT_MSGTYPE_COMMPREV;
    */

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

    SCOT_LOGALIGN_T* header, *pf_header;
    int pf_count = 0;

    __ht_try_insert(curr);
    __START_WRITE__ {

        latest = __ht_get_latest_entry(curr);

        // Am I marked as deleted? 
        // Or am I already replicated by prefetch?
        if (!IS_MARKED_AS_DELETED(latest->next) && (latest->ready != 0)) {

            SCOT_OPTIMIZATION_START {

                // Set prefetch size here.
                // Dead nodes will be notified by quroum_conns.
                size_t pf_size = ScotConnection::get_instance().get_quorum().size() + 1;
                size_t logsz = 0, pf_logsz = 0;

                header = log.write_local_log(latest); // At least, replicate this one.
                logsz = sizeof(struct ScotMessageHeader) + 
                    static_cast<size_t>(latest->buffer_sz) + sizeof(uint8_t);

                // Make this the first.
                reinterpret_cast<struct ScotMessageHeader*>(header)->msg |= SCOT_MSGTYPE_COMMPREV;

                for (int i = 1; i < pf_size; i++) {
                    
                    if ((index + i) == (SCOT_SLOT_COUNTS - 1))
                        break;

                    curr = slot.get_slot_entry(index + i);
                    if (curr->ready != 1)
                        break;

                    latest = __ht_get_latest_entry(curr);

                    // Record to local MR
                    pf_header = log.write_local_log(latest);
                    pf_logsz = sizeof(struct ScotMessageHeader) + 
                        static_cast<size_t>(latest->buffer_sz) + sizeof(uint8_t);

                    pf_count++;
                }

                if (pf_count > 0)
                    logsz = ((uintptr_t(pf_header) + pf_logsz) - uintptr_t(header));

#ifdef __DEBUG__
                __SCOT_INFO__(
                    msg_out, "→→ Fetch count: {}, write size ({}) ", (pf_count), logsz
                );
#endif

                size_t offset = uintptr_t(header) - uintptr_t(log.get_base());
                SCOT_LOGALIGN_T* remote_target_addr;

                bool ret = 0;

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
                        logsz,                      // Total message size
                        IBV_WR_RDMA_WRITE,
                        ctx.local.rpli_mr->lkey,    // Local MR acces key
                        ctx.remote.rply_mr->rkey,   // Remote MR access key
                        ctx.nid);
#ifdef __DEBUG__
                    assert(ret != false);
#endif
                }

                //
                // Wait for else.
                for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
                    ret = hartebeest_rdma_send_poll(ctx.local.rpli_qp);
#ifdef __DEBUG__
                    if (!ret) {
                        __SCOT_INFO__(msg_out, "→→ Polling error.");
                        assert(0);
                    }
#endif
                }
      
            } SCOT_OPTIMIZATION_END
        }
        else {
#ifdef __DEBUG__
            __SCOT_INFO__(msg_out, "→→ Pre-replicated, skipping.");
#endif
        }

        latest->ready = latest->blocked = 0;

#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ write_request end: {}", index);
#endif

    } __END_WRITE__

    if (slot.mark_entry_finished(index, latest, &hasht) == SCOT_SLOT_RESET) { // Reset comes here.
#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ Slot/hasht reset triggered");
#endif
    }
    
    return 0;
}