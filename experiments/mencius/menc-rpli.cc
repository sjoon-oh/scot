/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include "../../hartebeest/src/includes/hartebeest-c.h"
#include "../../src/includes/scot-def.hh"
#include "./includes/menc-core.hh"


#define SCOT_MENC_FIRST     0x00


scot_menc::ScotMenciusReplicator::ScotMenciusReplicator(SCOT_LOGALIGN_T* addr, int id) 
    : nid(id), ScotWriter(addr), msg_out("menc-rpli") { 
    
    sugg_lock.clear(std::memory_order_release);

    if (nid != SCOT_MENC_FIRST) {
        sugg_lock.test_and_set(std::memory_order_acquire);
#ifdef __DEBUG__
        __SCOT_INFO__(
            msg_out, "→ Replicator blocked for initial suggest.");
#endif
    } else {
#ifdef __DEBUG__
        __SCOT_INFO__(
            msg_out, "→ Replicator unblocked.");
#endif
    }
}


bool scot_menc::ScotMenciusReplicator::write_request(
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

    SCOT_LOGALIGN_T* header;
    
    if (msg == SCOT_MSGTYPE_PURE)
        while (sugg_lock.test_and_set(std::memory_order_acquire))
            ;

    __START_WRITE__ 
    {

        // Am I marked as deleted? 
        // Or am I already replicated by prefetch?
        if (!IS_MARKED_AS_DELETED(latest->next) && (latest->ready != 0)) {

            size_t logsz = 0;

            header = log.write_local_log(latest); // At least, replicate this one.
            logsz = sizeof(struct ScotMessageHeader) + 
                static_cast<size_t>(latest->buffer_sz) + sizeof(uint8_t);

            // Make this the first.
            reinterpret_cast<struct ScotMessageHeader*>(header)->msg |= SCOT_MSGTYPE_COMMPREV;

            size_t offset = uintptr_t(header) - uintptr_t(log.get_base());
            SCOT_LOGALIGN_T* remote_target_addr;

            bool ret = 0;

#ifdef __DEBUG__
            __SCOT_INFO__(msg_out, "→→ Suggest: {}, write size ({}) ", reinterpret_cast<char*>(buf), logsz);
#endif

            // 
            // Send WR first for all quorums
            for (auto& ctx: scot::ScotConnection::get_instance().get_quorum()) {
                
                remote_target_addr = 
                    reinterpret_cast<SCOT_LOGALIGN_T*>(
                            uintptr_t(ctx.remote.rply_mr->addr) + offset);

                ret = hartebeest_rdma_post_single_signaled_inline(
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
            for (auto& ctx: scot::ScotConnection::get_instance().get_quorum()) {
                ret = hartebeest_rdma_send_poll(ctx.local.rpli_qp);
#ifdef __DEBUG__
                if (!ret) {
                    __SCOT_INFO__(msg_out, "→→ Polling error.");
                    assert(0);
                }
#endif
            }
        }
        else {
#ifdef __DEBUG__
            __SCOT_INFO__(msg_out, "→→ Pre-replicated, skipping.");
#endif
        }

        latest->ready = latest->blocked = 0;

    } __END_WRITE__
    

    if (slot.mark_entry_finished(index, latest) == SCOT_SLOT_RESET) { // Reset comes here.
#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ Slot/hasht reset triggered");
#endif
    }
    
    return 0;
}


void scot_menc::ScotMenciusReplicator::allow_write() {
    sugg_lock.clear(std::memory_order_release);
}


bool scot_menc::ScotMenciusReplicator::try_write() {
    return sugg_lock.test_and_set(std::memory_order_acquire);
}