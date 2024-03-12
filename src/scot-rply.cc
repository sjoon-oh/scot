/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <regex>
#include <string>
#include <iostream>
#include <vector>
#include <queue>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


scot::ScotReplayer::ScotReplayer(
    uint32_t rply_id,                       // Receive from who?
    SCOT_LOGALIGN_T* base,                  // Global base of receiver MR
    scot::ScotConnContextPool* rpli_pool,   // For building offsets
    scot::ScotChecker* chkr_unlock,         // Unlock checkers
    SCOT_USRACTION ext_func)                // Replay function?
        : gbase(base), distr(), distr_signal(0), rnid(rply_id), chkr(chkr_unlock), user_action(ext_func) {

    // Extract offset.
    for (auto& ctx: rpli_pool->get_pool_list()) {
        base_list.push_back(
            reinterpret_cast<SCOT_LOGALIGN_T*>(uintptr_t(gbase) + ctx.at(0).offset));
    }

    distr_signal_toggle(SCOT_WRKR_PAUSE);
}


scot::ScotReplayer::~ScotReplayer() {
    distr_signal_toggle(SCOT_WRKR_HALT);

}


#define IS_SIGNALED(SIGN)   (distr_signal & SIGN)
void scot::ScotReplayer::__worker(
    uint32_t rply_id, 
    scot::ScotChecker* chkr, 
    SCOT_USRACTION ext_func) {

    std::string lc_name_out("rply-");
    lc_name_out += std::to_string(rply_id);

    MessageOut lc_out(lc_name_out.c_str());

    std::queue<std::thread> spawned;

    struct ScotMessageHeader* rcvd;
    SCOT_LOG_FINEGRAINED_T* pyld;

#ifdef __DEBUG__
    for (auto base: base_list)
        __SCOT_INFO__(
            lc_out, "base: 0x{:x}, aligned: 0x{:x}, idx: {}", 
                uintptr_t(base), 
                uintptr_t(reinterpret_cast<struct ScotAlignedLog*>(base)->aligned), 
                reinterpret_cast<struct ScotAlignedLog*>(base)->reserved[SCOT_RESERVED_IDX_FREE]
        );
#endif

    struct ReplayPair {
        SCOT_LOG_FINEGRAINED_T* buffer;
        size_t buffer_len;
    };

    // std::queue<struct ReplayPair> replayq_vec;
    
    std::vector<std::queue<struct ReplayPair>> replayq_vec;
    replayq_vec.resize(base_list.size());

    int idx = 0;
    while (1) {

        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;
        
        idx = 0;
        for (auto base: base_list) {

#ifdef __DEBUG__
            // __SCOT_INFO__(
            //     lc_out, "Before peek: 0x{:x}", uintptr_t(base)
            // );
#endif

            SCOT_LOG_FINEGRAINED_T* rcvd = 
                FINEPTR_LOOKALIKE(
                    peek_message(
                        reinterpret_cast<struct ScotAlignedLog*>(base), 
                        SCOT_MSGTYPE_PURE | SCOT_MSGTYPE_ACK
                    )
                );

#ifdef __DEBUG__
            // __SCOT_INFO__(
            //     lc_out, "After peek: 0x{:x}", uintptr_t(rcvd)
            // );
#endif

            // After peek, and if received the full message,
            if (rcvd != nullptr) {
                replayq_vec.at(idx).push({
                    .buffer = FINEPTR_LOOKALIKE(
                        uintptr_t(rcvd) + sizeof(struct ScotMessageHeader)),
                    .buffer_len = MSGHDRPTR_LOOKALIKE(rcvd)->buf_sz
                });

                chkr->release_wait(MSGHDRPTR_LOOKALIKE(rcvd)->hashv);

                // Here, insert to hashtable.

                // If commit, get latest.
                if (MSGHDRPTR_LOOKALIKE(rcvd)->msg & SCOT_MSGTYPE_COMMPREV) {
                    
                    if (replayq_vec.at(idx).size() > 0) {
                        // Launch thread here.

                        SCOT_LOG_FINEGRAINED_T* rep_buf = replayq_vec.at(idx).front().buffer;
                        size_t rep_buflen = replayq_vec.at(idx).front().buffer_len;

                        spawned.push(std::thread(
                            [&]() -> void { 
                                if (ext_func != nullptr)
                                    ext_func(rep_buf, rep_buflen);
                                    
                                MSGHDRPTR_LOOKALIKE(rcvd)->msg = SCOT_MSGTYPE_NONE;
                            }));
                    }
                }
            }

            idx++;
        }

        // 
        // Spawn one in each qpool segment,
        //  to serialize.
        if (spawned.size() != 0) {
            while (spawned.size() != 0) {
#ifdef __DEBUG__
                __SCOT_INFO__(lc_out, "→ Waiting spawns to finish, left: {}", spawned.size());
#endif
                spawned.front().join();
                spawned.pop();
            }

            for (auto& elemq: replayq_vec)
                elemq.pop();
        }
    }
}


void scot::ScotReplayer::distr_spawn() {
    distr = std::thread([&]() {
        __worker(rnid, chkr, user_action);
    });

    distr.detach();
}


void scot::ScotReplayer::distr_signal_toggle(uint32_t signal) {
    distr_signal ^= signal;
}