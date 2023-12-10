#include <cstdlib>
#include <cstdio>

#include "./includes/lfmap.hh"

#define ROTATE_LEFT_32(x, r)        ((x << r) | (x >> (32 - r)))


struct List* scot::hash::LockfreeMap::__bucket_init() {
    return reinterpret_cast<struct List*>(
            aligned_alloc(8, sizeof(struct List) * nelem));
}


void scot::hash::LockfreeMap::__list_init(struct List* arg_list) {
    arg_list->head.next = &arg_list->head;
}


bool scot::hash::LockfreeMap::__elem_insert(
    struct ScotSlotEntry* arg_new, struct ScotSlotEntry* arg_prev, struct ScotSlotEntry* arg_next) {

    do {
        struct ScotSlotEntry* orig_next;

        arg_new->next = arg_next; 
        orig_next = CASV(&(arg_prev->next), arg_next, arg_new);

        if (orig_next == arg_next) return true;
        if (orig_next != GET_MARKED_AS_PROTECTED(arg_next))
            return false;

        arg_next = orig_next;

    } while (1);
}


bool scot::hash::LockfreeMap::__elem_switch(
    struct ScotSlotEntry* arg_new, struct ScotSlotEntry* arg_prev, struct ScotSlotEntry* arg_next) {

    do {
        struct ScotSlotEntry* orig_next;

        arg_new->next = arg_next;  

        orig_next = CASV(&(arg_prev->next), 
            GET_MARKED_AS_PROTECTED(arg_next), 
            GET_MARKED_AS_DELETED(arg_new));

        if (orig_next == GET_MARKED_AS_PROTECTED(arg_next)) return true;
        if (!IS_MARKED_AS_PROTECTED(orig_next)) return false;

        arg_next = GET_UNMARKED_REFERENCE(orig_next); 

    } while (1);
}


void scot::hash::LockfreeMap::__elem_delete(struct ScotSlotEntry* arg_target) {

    struct ScotSlotEntry* next = arg_target->next;

    do {
        struct ScotSlotEntry* orig_next;

        if (IS_MARKED_REFERENCE(next, ALL)) return; 

        orig_next = CASV(&(arg_target->next), next, GET_MARKED_AS_DELETED(next));
        
        if (IS_SAME_BASEREF(orig_next, next))
            return;

        next = orig_next;

    } while (1);   
}


void scot::hash::LockfreeMap::__elem_cleanups(struct List* arg_list) {

    struct ScotSlotEntry *curr, *prev;           // current cell & previous cell.  
    struct ScotSlotEntry *curr_next, *prev_next; // 

    prev = &arg_list->head;
    curr = prev->next; 
    curr_next = curr->next;

    while (curr != &arg_list->head) {

        if (!IS_MARKED_AS_DELETED(curr_next)) {
            if (!IS_SAME_BASEREF(prev_next, curr))
                CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));

            prev = curr; 
            prev_next = prev->next; 
        }

        curr = GET_UNMARKED_REFERENCE(curr_next);
        curr_next = curr->next;
    }

    if (!IS_SAME_BASEREF(prev_next, curr))
        CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));
}


void scot::hash::LockfreeMap::__elem_cleanup_after_slot(struct List* arg_list, struct ScotSlotEntry* arg_slot) {

    struct ScotSlotEntry *curr, *prev;           // current cell & previous cell.  
    struct ScotSlotEntry *curr_next, *prev_next; // 

    prev = arg_slot;
    curr = prev->next; 
    curr_next = curr->next;

    while (curr != &arg_list->head) {

        if (!IS_MARKED_AS_DELETED(curr_next)) {
            if (!IS_SAME_BASEREF(prev_next, curr))
                CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));

            prev = curr; 
            prev_next = prev->next; 
        }

        curr = GET_UNMARKED_REFERENCE(curr_next);
        curr_next = curr->next;
    }

    if (!IS_SAME_BASEREF(prev_next, curr))
        CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));
}


struct List* scot::hash::LockfreeMap::__get_bucket(uint32_t arg_idx) {
    
    if (arg_idx >= nelem) arg_idx %= nelem;
    return &bucket[arg_idx];
}


bool scot::hash::LockfreeMap::__elemSearch(
    uint32_t arg_hashval, struct ScotSlotEntry* arg_key, struct ScotSlotEntry** arg_left, struct ScotSlotEntry** arg_right) {
        
    struct List* list = __get_bucket(arg_hashval);

    struct ScotSlotEntry* prev       = &list->head;
    struct ScotSlotEntry* prev_next  = prev->next;
    struct ScotSlotEntry* curr       = prev->next; 
    struct ScotSlotEntry* curr_next  = curr->next;

    while (curr != &list->head && compare_func(curr, arg_key) <= 0) {
        
        if (!IS_MARKED_AS_DELETED(curr_next)) {
            if (!IS_SAME_BASEREF(prev_next, curr)) {
                //
                // Mod: Deleted one line.
                //  This modification do not clean up the deleted-marked elements.
                //  It just leaves there.
                //
                // CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));
            }

            if (compare_func(arg_key, curr) == 0) {                
                do {
                    struct ScotSlotEntry* orig_curr_next; 

                    orig_curr_next = CASV(&(curr->next), curr_next, 
                        GET_MARKED_AS_PROTECTED(curr_next));

                    if (orig_curr_next == curr_next || 
                        orig_curr_next == GET_MARKED_AS_PROTECTED(curr_next)) 
                    {
                        *arg_left  = curr;
                        *arg_right = GET_UNMARKED_REFERENCE(curr_next); 
                        return true; 
                    }

                    curr_next = orig_curr_next;

                } while (!IS_MARKED_AS_DELETED(curr_next));
            }

            prev = curr; 
            prev_next = prev->next; 
        }

        curr = GET_UNMARKED_REFERENCE(curr_next);
        curr_next = curr->next;
    }

    if (!IS_SAME_BASEREF(prev_next, curr)) {
        CAS(&(prev->next), prev_next, GET_SAME_MARK(curr, prev_next));
    }

    *arg_left  = prev; 
    *arg_right = curr; 
    
    return false;
}


uint32_t scot::hash::LockfreeMap::__murmur_hash3(const void* arg_key, int arg_len, uint32_t arg_seed) {

    const uint8_t* data = reinterpret_cast<const uint8_t*>(arg_key);
    const int nblocks = arg_len / 4;

    uint32_t h1 = arg_seed;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    uint32_t k1;

    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks * 4);
    
    for (int i = -nblocks; i; i++) {
        k1 = blocks[i];
        k1 *= c1; 

        k1 = ROTATE_LEFT_32(k1, 15); 
        k1 *= c2;

        h1 ^= k1; 
        h1 = ROTATE_LEFT_32(h1, 13); 
        h1 = h1 * 5 + 0xe6546b64;
    }

    // For last 1 ~ 3 byte ==> Tail
    const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);

    k1 = 0;

    switch (arg_len & 3) {
        case 3:     k1 ^= tail[2] << 16;
        case 2:     k1 ^= tail[1] << 8;
        case 1:     k1 ^= tail[0];
        
        k1 *= c1; 
        k1 = ROTATE_LEFT_32(k1, 15); 
        k1 *= c2; 
        h1 ^= k1;
    }

    h1 ^= arg_len; 

    h1 ^= (h1 >> 16);
    h1 *= 0x85ebca6b;
    h1 ^= (h1 >> 13);
    h1 *= 0xc2b2ae35;
    h1 ^= (h1 >> 16);

    return h1;
}


scot::hash::LockfreeMap::LockfreeMap(uint32_t arg_nelem, compfunc_t arg_compfunc) 
    : nelem(arg_nelem), compare_func(arg_compfunc) {

    // Allocate buckets
    if ((bucket = __bucket_init()) == nullptr) abort();

    // Init lists
    for (int idx = 0; idx < nelem; idx++)
        __list_init(&bucket[idx]);
}


scot::hash::LockfreeMap::~LockfreeMap() {
    free(bucket);
}


uint32_t scot::hash::LockfreeMap::hash(const void* arg_key, int arg_len) {
    return __murmur_hash3(arg_key, arg_len, 0);
}


bool scot::hash::LockfreeMap::insert(struct ScotSlotEntry* arg_new, struct ScotSlotEntry* arg_prev, struct ScotSlotEntry* arg_next) {
    return __elem_insert(arg_new, arg_prev, arg_next);
}


bool scot::hash::LockfreeMap::replace(struct ScotSlotEntry* arg_new, struct ScotSlotEntry* arg_prev, struct ScotSlotEntry* arg_next) {
    return __elem_switch(arg_new, arg_prev, arg_next);
}


bool scot::hash::LockfreeMap::search(
    uint32_t arg_hashval, struct ScotSlotEntry* arg_key, struct ScotSlotEntry** arg_left, struct ScotSlotEntry** arg_right) {
    
    return __elemSearch(arg_hashval, arg_key, arg_left, arg_right);
}


struct List* scot::hash::LockfreeMap::get_bucket(uint32_t arg_idx) {
    return __get_bucket(arg_idx);
}


struct List* scot::hash::LockfreeMap::get_bucket_by_idx(uint32_t arg_idx) {
    return &bucket[arg_idx];
}


void scot::hash::LockfreeMap::remove(struct ScotSlotEntry* arg_target) {
    __elem_delete(arg_target);
}


void scot::hash::LockfreeMap::cleanup(struct List* arg_list) {
    __elem_cleanups(arg_list);
}


void scot::hash::LockfreeMap::cleanup_after_slot(struct List* arg_list, struct ScotSlotEntry* arg_slot) {
    __elem_cleanup_after_slot(arg_list, arg_slot);
}


void scot::hash::LockfreeMap::reset() {
    // Init lists
    for (int idx = 0; idx < nelem; idx++)
        __list_init(&bucket[idx]);
}