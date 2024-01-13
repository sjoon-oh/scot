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


scot::ScotReplayer::ScotReplayer(SCOT_LOGALIGN_T* addr, uint32_t rply_id, scot::ScotChecker* chkr_unlock) 
    : ScotReader(addr), worker(), worker_signal(0), id(rply_id), chkr(chkr_unlock) { 

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


scot::ScotReplayer::~ScotReplayer() {
    worker_signal_toggle(SCOT_WRKR_HALT);
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)
void scot::ScotReplayer::__worker(
    struct ScotLog* log, uint32_t rply_id, scot::ScotChecker* chkr) {

    std::string lc_name_out("rply-");
    lc_name_out += std::to_string(rply_id);

    MessageOut lc_out(lc_name_out.c_str());

    // Log base
#ifdef __DEBUG__
    SCOT_LOGALIGN_T* log_base = log->get_base();
    __SCOT_INFO__(lc_out, "{} spawned. Log base starts at {:x}.", 
        lc_name_out, uintptr_t(log_base));
#endif

    struct ScotMessageHeader* rcvd;
    SCOT_LOG_FINEGRAINED_T* pyld;

    while (1) {

        // Pause when signaled.
        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        // End point, whether it is pure, or ACKed.
        rcvd = reinterpret_cast<struct ScotMessageHeader*>(
            log->poll_next_local_log(SCOT_MSGTYPE_PURE | SCOT_MSGTYPE_ACK)
        );
        
        pyld = reinterpret_cast<SCOT_LOG_FINEGRAINED_T*>(rcvd) 
            + uintptr_t(sizeof(struct ScotMessageHeader));

        if (rcvd->msg & SCOT_MSGTYPE_ACK) {
            chkr->release_wait(rcvd->hashv);
        }

        if (rcvd->msg & SCOT_MSGTYPE_COMMPREV) {
            
            // Replay function goes here.

#ifdef __DEBUG__
            __SCOT_INFO__(lc_out, "→ Commit piggybacked. Safe to replay.");
#endif
        }

        rcvd->msg = SCOT_MSGTYPE_NONE;

    }
}


void scot::ScotReplayer::worker_spawn() {
    worker = std::thread([&](){ __worker(&(this->log), this->id, this->chkr); });
    worker.detach();
}


void scot::ScotReplayer::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}