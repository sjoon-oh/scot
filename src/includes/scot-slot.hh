#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <stdint.h>
#include <time.h>

#include <atomic>


#include <infiniband/verbs.h>

#include "./scot-def.hh"


#ifdef __cplusplus
extern "C" {
#endif

    struct ScotSlotEntry {
        struct ScotSlotEntry* next;
        uint32_t hashv;

        uint8_t ready;
        uint8_t blocked;

        uint8_t* buffer;
        uint16_t buffer_sz;

        uint8_t* key;
        uint16_t key_sz;

        uint8_t msg;
    };

    // Minimum
    struct ScotSlotEntryArgs {
        uint32_t hashv;
        uint8_t* buffer;
        uint16_t buffer_sz;
        uint8_t* key;
        uint16_t key_sz;
        uint8_t msg;
    };

#ifdef __cplusplus
}
#endif

namespace scot {

    namespace hash {
        class LockfreeMap;
    }

    class ScotSlot {
    private:

        struct ScotSlotEntry slot[SCOT_SLOT_COUNTS];
        std::atomic_flag slot_reset_lock;

        std::atomic_uint next_free;
        std::atomic_uint processed;

        void __wait_until_proc_finish();
        void __wait_until_reset_finish();

        uint32_t __get_next_slot();
        

    public:
        ScotSlot();
        ~ScotSlot() = default;

        uint32_t register_entry(struct ScotSlotEntryArgs);
        int mark_entry_finished(uint32_t index, struct ScotSlotEntry*, hash::LockfreeMap* = nullptr);

        struct ScotSlotEntry* get_slot_entry(uint32_t);
        struct ScotSlotEntry* get_next_unfinished();

        uint32_t peek_processed();
        uint32_t peek_next_free();
    };
}