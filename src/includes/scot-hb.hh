#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <map>
#include <string>
#include <vector>

#include <thread>

#include <infiniband/verbs.h>

#include "./scot-def.hh"


#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((packed)) ScotScoreboard {
    SCOT_SCB_WIDE_T score;              // 2 Byte
    SCOT_SCB_FINEGRAINED_T status;      // 1 Byte
    SCOT_SCB_FINEGRAINED_T reserved;    // 1 Byte
};

#ifdef __cplusplus
}
#endif


namespace scot {

    struct ScotScoreboardTable {
        struct ScotScoreboard table[SCOT_MAX_QSIZE];
    };
    

    class ScotHeartbeat {
    private:
        SCOT_SCB_FINEGRAINED_T* scb_open;
        struct ScotScoreboardTable scb_table;

        std::thread beater;

        void __beater(SCOT_SCB_FINEGRAINED_T*, struct ScotScoreboardTable*);
        
    public:
        ScotHeartbeat(SCOT_SCB_FINEGRAINED_T* = nullptr);
        ~ScotHeartbeat();

        void beater_spawn();
        void register_scb(SCOT_SCB_FINEGRAINED_T*);
    };
}
