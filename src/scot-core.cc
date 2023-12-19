
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


scot::ScotWriter::ScotWriter(struct ScotAlignedLog* addr) : log(addr) {
    writer_lock.clear();
}


scot::ScotReader::ScotReader(struct ScotAlignedLog* addr) : log(addr) { }


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


scot::ScotReplicator::ScotReplicator(struct ScotAlignedLog* addr) 
    : ScotWriter(addr), hasht(SCOT_HT_SIZE, scot_hash_cmp) { 
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

        SCOT_LOGALIGN_T* log_pos = log.write_local_log(latest); // Write to the local log here.        
        size_t log_sz = sizeof(struct ScotMessageHeader) + latest->buffer_sz + sizeof(uint8_t);

        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {

            hartebeest_rdma_post_single_fast(
                ctx.local.rpli_qp, 
                log_pos, ctx.remote.rply_mr->addr, log_sz, IBV_WR_RDMA_WRITE,
                ctx.remote.rply_mr->lkey, ctx.remote.rply_mr->rkey, 0);

            hartebeest_rdma_send_poll(ctx.local.rpli_qp);
        }

        slot.mark_entry_finished(index, latest);

    } __END_WRITE__
    
    return 0;
}


scot::ScotReplayer::ScotReplayer(struct ScotAlignedLog* addr) : ScotReader(addr) { 

}


void scot::ScotReplayer::spawn_worker(SCOT_REPLAYER_WORKER_T routine) {

    struct ScotLog* log_inst = &log;

    std::thread _worker(routine, log_inst);
    _worker.detach();

    worker = std::move(_worker);
}


scot::ScotCore::ScotCore() {

    // Initialize connection
    ScotConnection::get_instance().start();

    int nid = ScotConnection::get_instance().get_my_nid();
    // int qsize = ScotConnection::get_instance().get_quorum_sz();

    rpli = new ScotReplicator(
            reinterpret_cast<struct ScotAlignedLog*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, wkey_helper(HBKEY_MR_RPLI).c_str())->addr)
    );

    // vec_rply.resize(qsize, nullptr);
    for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
        vec_rply.push_back(
            new ScotReplayer(
                reinterpret_cast<struct ScotAlignedLog*>(
                    hartebeest_get_local_mr(
                        HBKEY_PD, rkey_helper(HBKEY_MR_RPLY, nid, ctx.nid).c_str())->addr)
            )
        );
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

    std::cout << "Propose. \n";

    // Any rules.

    rpli->write_request(
        buf, buf_sz, key, key_sz, hashv, msg
    );
    
    std::cout << "Propose end. \n";

    return 0;
};


void scot::ScotCore::initialize() { };
void scot::ScotCore::finish() { };