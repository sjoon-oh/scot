
#include <regex>
#include <string>
#include <iostream>
#include <vector>


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
    uint8_t* buf, uint16_t buf_sz, uint8_t* key, uint16_t key_sz, 
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


        // Write to the local log here.

        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
            // hartebeest_rdma_post_single_fast(
                
            //     ctx.local.rpli_qp, 


            //     ctx.remote.rply_mr->addr



            //     // ctx.local_qp, header, remote_header, write_sz, IBV_WR_RDMA_WRITE, rsdco_rpli_mr->lkey, ctx.remote_mr->rkey, 0
            // );
        }

        slot.mark_entry_finished(index, latest);

    } __END_WRITE__
    
    return 0;
}


scot::ScotCore::ScotCore() {
    ScotConnection::get_instance().start();
}


void scot::ScotCore::initialize() { };
void scot::ScotCore::finish() { };