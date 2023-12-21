
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"

// Configuration
scot::ScotConfLoader::ScotConfLoader() {

}


scot::ScotWriter::ScotWriter(SCOT_LOGALIGN_T* addr) : log(addr) {
    writer_lock.clear();
}


SCOT_LOGALIGN_T* scot::ScotWriter::get_base() {
    return log.get_base();
}


scot::ScotReader::ScotReader(
        SCOT_LOGALIGN_T* addr) 
    : log(addr) {

}


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

            bool ret = hartebeest_rdma_post_single_fast(
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
            hartebeest_rdma_send_poll(ctx.local.rpli_qp);
        }

        slot.mark_entry_finished(index, latest);

    } __END_WRITE__
    
    return 0;
}


scot::ScotReplayer::ScotReplayer(SCOT_LOGALIGN_T* addr, uint32_t rply_id) 
    : ScotReader(addr), worker(), worker_signal(0), id(rply_id) { 

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


scot::ScotReplayer::~ScotReplayer() {
    worker_signal_toggle(SCOT_WRKR_HALT);
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)
void scot::ScotReplayer::__worker(struct ScotLog* log, uint32_t rply_id) {

    std::string lc_name_out("rply-");
    lc_name_out += std::to_string(rply_id);

    MessageOut lc_out(lc_name_out.c_str());

    // Log base
#ifdef _ON_DEBUG_
    SCOT_LOGALIGN_T* log_base = log->get_base();
    __SCOT_INFO__(lc_out, "{} spawned. Log base starts at {:x}.", 
        lc_name_out, uintptr_t(log_base));
#endif

    while (1) {

        // Pause when signaled.
        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        __SCOT_INFO__(lc_out, "→ Waiting for instance: {}", log->get_instn());

        log->poll_next_local_log(SCOT_MSGTYPE_PURE);












    }
}


void scot::ScotReplayer::worker_spawn() {
    worker = std::thread([&](){ __worker(&(this->log), this->id); });
    worker.detach();
}


void scot::ScotReplayer::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}


scot::ScotCore::ScotCore() : msg_out("scot-core") {

    __SCOT_INFO__(msg_out, "Core init start.");

    // Initialize connection
    ScotConnection::get_instance().start();

    int nid = ScotConnection::get_instance().get_my_nid();
    // int qsize = ScotConnection::get_instance().get_quorum_sz();

    rpli = new ScotReplicator(
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, wkey_helper(HBKEY_MR_RPLI).c_str())->addr)
    );

    __SCOT_INFO__(msg_out, "→ A Replicator initiated: 0x{:x}", uintptr_t(rpli));
    
    struct ibv_mr* mr = nullptr;
    for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
        
        mr = hartebeest_get_local_mr(HBKEY_PD, rkey_helper(HBKEY_MR_RPLY, nid, ctx.nid).c_str());
        vec_rply.push_back(
            new ScotReplayer(reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid)
        );

        vec_rply.back()->worker_spawn();
    }

    __SCOT_INFO__(msg_out, "→ All Replayers spawned.");

    for (auto& rply: vec_rply) {
        rply->worker_signal_toggle(SCOT_WRKR_PAUSE); // Disable pause
    }

}

scot::ScotCore::~ScotCore() {
    if (rpli != nullptr) delete rpli;
    for (auto& rply: vec_rply) 
        if (rply != nullptr) delete rply;
}


int scot::ScotCore::propose(
    uint8_t* buf, uint16_t buf_sz, uint8_t* key, uint16_t key_sz, 
    uint32_t hashv, uint8_t msg) {

#ifdef _ON_DEBUG_
    __SCOT_INFO__(msg_out, "→ PROPOSE called.");
#endif

    // Rules?

    rpli->write_request(
        buf, buf_sz, key, key_sz, hashv, msg
    );

#ifdef _ON_DEBUG_
    __SCOT_INFO__(msg_out, "→ PROPOSE finished.");
#endif

    return 0;
};


void scot::ScotCore::initialize() { };
void scot::ScotCore::finish() { 

};