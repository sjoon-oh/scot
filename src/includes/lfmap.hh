#pragma once
/* github.com/sjoon-oh/scot
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project scot
 */

//
// This codes are originated from Dohyun Kim (ehgus421210@kaist.ac.kr),
//  slightly refactored.

#include <cstdint>

#include "./scot-log.hh"
#include "./scot-slot.hh"

//
// Basic building blocks.
#define CAS(p, o, n)    __sync_bool_compare_and_swap((uint64_t *) p, o, n)
#define CASV(p, o, n)   ((typeof(*p)) __sync_val_compare_and_swap((uint64_t *)p, o, n))
#define FAA(p, v)       ((typeof(p)) __sync_fetch_and_add((uint64_t *) &p, v))

#define XXX             0x1
#define YYY             0x2
#define ALL             (XXX | YYY)

#define IS_MARKED_REFERENCE(c, m)       ((((uint64_t) (c)) & (m)) != 0)
#define IS_MARKED_AS_DELETED(c)         IS_MARKED_REFERENCE(c, XXX)
#define IS_MARKED_AS_PROTECTED(c)       IS_MARKED_REFERENCE(c, YYY)

#define GET_UNMARKED_REFERENCE(c)       (typeof(c))(((uint64_t) (c)) & ~ALL)
#define GET_MARKED_REFERENCE(c, m)      (typeof(c))((((uint64_t) (c)) & ~ALL) | (m))
#define GET_MARKED_AS_DELETED(c)        GET_MARKED_REFERENCE(c, XXX)
#define GET_MARKED_AS_PROTECTED(c)      GET_MARKED_REFERENCE(c, YYY)

#define IS_SAME_BASEREF(a, b)           (GET_UNMARKED_REFERENCE(a) == GET_UNMARKED_REFERENCE(b))
#define GET_SAME_MARK(a, b)             (typeof(a))(((uint64_t) (a)) | (((uint64_t) (b)) & ALL))


typedef int32_t (*compfunc_t) (void *, void *);

struct __attribute__((packed)) Cell {
    struct ScotSlotEntry* next; 
};

struct __attribute__((packed)) List {
    struct ScotSlotEntry head;
};


//
// Wrapping start.
namespace scot {
    namespace hash {

        class LockfreeMap {
        private:
            uint32_t        nelem;
            struct List*    bucket;

            compfunc_t      compare_func;

            //
            // Initialization functions
            struct List* __bucket_init();        // Initializes buckets
            void __list_init(struct List*);      // Initializes the list

            // Modification Interfaces
            bool __elem_insert(struct ScotSlotEntry*, struct ScotSlotEntry*, struct ScotSlotEntry*);
            bool __elem_switch(struct ScotSlotEntry*, struct ScotSlotEntry*, struct ScotSlotEntry*);
            void __elem_delete(struct ScotSlotEntry*);
            void __elem_cleanups(struct List*);
            void __elem_cleanup_after_slot(struct List*, struct ScotSlotEntry*);

            struct List* __get_bucket(uint32_t);
            bool __elemSearch(uint32_t, struct ScotSlotEntry*, struct ScotSlotEntry**, struct ScotSlotEntry**);


            // Core hash.
            uint32_t __murmur_hash3(const void*, int, uint32_t);
            
        public:

            LockfreeMap(uint32_t, compfunc_t);
            ~LockfreeMap();
            
            uint32_t hash(const void*, int);

            // Interfaces.
            bool insert(struct ScotSlotEntry*, struct ScotSlotEntry*, struct ScotSlotEntry*);
            bool replace(struct ScotSlotEntry*, struct ScotSlotEntry*, struct ScotSlotEntry*);
            bool search(uint32_t, struct ScotSlotEntry*, struct ScotSlotEntry**, struct ScotSlotEntry**);
            void remove(struct ScotSlotEntry*);
            void cleanup(struct List*);
            void cleanup_after_slot(struct List*, struct ScotSlotEntry*);

            void reset();

            struct List* get_bucket(uint32_t);
            struct List* get_bucket_by_idx(uint32_t);
        };

    }
}
