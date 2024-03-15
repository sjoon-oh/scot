/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"




scot::ScotReceiver::ScotReceiver(
    uint32_t rply_id,                       // Receive from who?
    SCOT_LOGALIGN_T* base,                  // Global base of receiver MR
    scot::ScotConnContextPool* chkr_pool,   // For building offsets
    scot::ScotReplicator* rpli_hdl,         // Replicator handle
    SCOT_USRACTION ext_func)                // Replay function?
        : gbase(base), worker(), worker_signal(0), rnid(rply_id), rpli(rpli_hdl), user_action(ext_func) {

    // Extract offset.
    for (auto& ctx: chkr_pool->get_pool_list()) {
        base_list.push_back(
            reinterpret_cast<SCOT_LOGALIGN_T*>(uintptr_t(gbase) + ctx.at(0).offset));
    }

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


// scot::ScotReceiver::ScotReceiver(SCOT_LOGALIGN_T* addr, uint32_t rcvr_id, scot::ScotReplicator* rpli_ack, SCOT_USRACTION ext_func) 
//     : ScotReader(addr), worker(), worker_signal(0), id(rcvr_id), rpli(rpli_ack), user_action(ext_func) { 

//     worker_signal_toggle(SCOT_WRKR_PAUSE);
// }


scot::ScotReceiver::~ScotReceiver() {
    worker_signal_toggle(SCOT_WRKR_HALT);
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)

void scot::ScotReceiver::__worker(
    uint32_t rcvr_id, 
    scot::ScotReplicator* rpli, 
    SCOT_USRACTION ext_func) {

// void scot::ScotReceiver::__worker(
//     struct ScotLog* log, uint32_t rcvr_id, scot::ScotReplicator* rpli, SCOT_USRACTION ext_func) {

    std::string lc_name_out("rcvr-");
    lc_name_out += std::to_string(rcvr_id);

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

    int idx = 0;
    while (1) {

        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        idx = 0;
        for (auto base: base_list) {

            SCOT_LOG_FINEGRAINED_T* rcvd = 
                FINEPTR_LOOKALIKE(
                    peek_message(
                        reinterpret_cast<struct ScotAlignedLog*>(base), 
                        SCOT_MSGTYPE_WAIT | SCOT_MSGTYPE_HDRONLY
                    )
                );

            pyld = FINEPTR_LOOKALIKE(rcvd) + uintptr_t(sizeof(struct ScotMessageHeader));

            //
            // After peek, and if received the full message,
            if (rcvd != nullptr) {
                if (reinterpret_cast<struct ScotMessageHeader*>(rcvd)->msg != SCOT_MSGTYPE_HDRONLY ) {

#ifdef __DEBUG__
                    uint32_t hashv_local = reinterpret_cast<struct ScotMessageHeader*>(rcvd)->hashv;
                    __SCOT_INFO__(
                        lc_out, "ACK as RPLI, hashv: {}, pyld: {}", hashv_local, (char*)pyld
                    );
#endif
                    spawned.push(
                        std::move(
                            std::thread(
                            [=]() -> void { 
                                rpli->write_request(
                                    pyld, 
                                    MSGHDRPTR_LOOKALIKE(rcvd)->buf_sz, 
                                    nullptr, 
                                    0, 
                                    MSGHDRPTR_LOOKALIKE(rcvd)->hashv, 
                                    SCOT_MSGTYPE_ACK
                                );

                                if (ext_func != nullptr)
                                    ext_func(rcvd, MSGHDRPTR_LOOKALIKE(rcvd)->buf_sz);
                                
                                MSGHDRPTR_LOOKALIKE(rcvd)->msg = SCOT_MSGTYPE_NONE;
                            }))
                    );
                }
            }
        }

        while (!spawned.empty()) {
#ifdef __DEBUG__
            __SCOT_INFO__(lc_out, "→ Waiting spawns to finish, left: {}", spawned.size());
#endif
            spawned.front().join();
            spawned.pop();
#ifdef __DEBUG__
            __SCOT_INFO__(lc_out, "→ Front spawn finished, left: {}", spawned.size());
#endif

        }
    }
}


void scot::ScotReceiver::worker_spawn() {
    worker = std::thread([&]() {
        __worker(rnid, rpli, user_action);
    });

    worker.detach();
}


void scot::ScotReceiver::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}