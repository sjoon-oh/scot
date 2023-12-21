#include <vector>
#include <string>
#include <memory>
#include <atomic>

#include <iostream>

#include <cstring>

#include <assert.h>
#include <stdlib.h>

#include "./includes/scot-slot.hh"
#include "./includes/lfmap.hh"

scot::ScotSlot::ScotSlot() {
 
    slot_reset_lock.clear();  // Set to false

    next_free.store(0);
    processed.store(0);
}


void scot::ScotSlot::__wait_until_proc_finish() {
    uint32_t procd;
    while ((procd = processed.load()) != SCOT_SLOT_COUNTS)
        ;
}


void scot::ScotSlot::__wait_until_reset_finish() {
    uint32_t nf;
    while (!((nf = next_free.load()) < SCOT_SLOT_COUNTS))
        ;
}


uint32_t scot::ScotSlot::__get_next_slot() {

    uint32_t next_index = next_free.fetch_add(1);
    if (next_index < SCOT_SLOT_COUNTS)
        return next_index;

    __wait_until_reset_finish();

    return __get_next_slot();
}


uint32_t scot::ScotSlot::register_entry(struct ScotSlotEntryArgs sa) {

    uint32_t index = __get_next_slot();
    
    slot[index].hashv       = sa.hashv;
    slot[index].ready       = 1;
    slot[index].blocked     = 1;
    slot[index].buffer      = sa.buffer;
    slot[index].buffer_sz   = sa.buffer_sz;
    slot[index].key         = sa.key;
    slot[index].key_sz      = sa.key_sz;
    slot[index].msg         = sa.msg;

    return index;
}


int scot::ScotSlot::mark_entry_finished(
        uint32_t index, struct ScotSlotEntry* procd_entry, 
        hash::LockfreeMap* ht
    ) {
    
    uint32_t proc = processed.fetch_add(1); // Record currently processed slot number
    procd_entry->ready = 0;  

    // If last index, reset all slots.
    if (index == (SCOT_SLOT_COUNTS - 1)) {

        __wait_until_proc_finish();

        if (ht != nullptr)
            ht->reset();

        std::memset(&slot, 0, sizeof(struct ScotSlotEntry) * SCOT_SLOT_COUNTS);

        next_free.store(0);
        processed.store(0);

        return SCOT_SLOT_RESET;
    }

    return 0;
}


struct ScotSlotEntry* scot::ScotSlot::get_slot_entry(uint32_t index) {
    return &slot[index];
}


struct ScotSlotEntry* scot::ScotSlot::get_next_unfinished() {

    uint32_t next = processed.fetch_add(1);
    return &slot[next];
}


uint32_t scot::ScotSlot::peek_next_free() {
    return next_free.load();
}


uint32_t scot::ScotSlot::peek_processed() {
    return processed.load();
}