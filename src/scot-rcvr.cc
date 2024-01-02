
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


scot::ScotReceiver::ScotReceiver(SCOT_LOGALIGN_T* addr, uint32_t rcvr_id, scot::ScotReplicator* rpli_ack) 
    : ScotReader(addr), worker(), worker_signal(0), id(rcvr_id), rpli(rpli_ack) { 

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


scot::ScotReceiver::~ScotReceiver() {
    worker_signal_toggle(SCOT_WRKR_HALT);
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)
void scot::ScotReceiver::__worker(
    struct ScotLog* log, uint32_t rcvr_id, scot::ScotReplicator* rpli) {

    std::string lc_name_out("rcvr-");
    lc_name_out += std::to_string(rcvr_id);

    MessageOut lc_out(lc_name_out.c_str());

    // Log base
#ifdef __DEBUG__X
    SCOT_LOGALIGN_T* log_base = log->get_base();
    __SCOT_INFO__(lc_out, "{} spawned. Log base starts at {:x}.", 
        lc_name_out, uintptr_t(log_base));
#endif

    struct ScotMessageHeader* rcvd;
    SCOT_LOG_FINEGRAINED_T* pyld;

    while (1) {

        // Pause when signaled.
        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        // Wait for WAIT.
        rcvd = reinterpret_cast<struct ScotMessageHeader*>(
            log->poll_next_local_log(SCOT_MSGTYPE_WAIT | SCOT_MSGTYPE_HDRONLY)
        );

        pyld = reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(rcvd) 
            + uintptr_t(sizeof(struct ScotMessageHeader));

        if (rcvd->msg == SCOT_MSGTYPE_WAIT) {
            rpli->write_request(
                pyld, rcvd->buf_sz, nullptr, 0, rcvd->hashv, SCOT_MSGTYPE_ACK
            );

#ifdef __DEBUG__X
            uint32_t received_hash = rcvd->hashv;
            __SCOT_INFO__(lc_out, "→ Replying as ACK: {}", received_hash);
#endif

        rcvd->msg = SCOT_MSGTYPE_NONE;

        }
    }
}


void scot::ScotReceiver::worker_spawn() {
    worker = std::thread([&](){ __worker(&(this->log), this->id, this->rpli); });
    worker.detach();
}


void scot::ScotReceiver::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}