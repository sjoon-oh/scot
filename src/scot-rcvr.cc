
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


scot::ScotReceiver::ScotReceiver(SCOT_LOGALIGN_T* addr, uint32_t rcvr_id) 
    : ScotReader(addr), worker(), worker_signal(0), id(rcvr_id) { 

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


scot::ScotReceiver::~ScotReceiver() {
    worker_signal_toggle(SCOT_WRKR_HALT);
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)
void scot::ScotReceiver::__worker(struct ScotLog* log, uint32_t rcvr_id) {

    std::string lc_name_out("rcvr-");
    lc_name_out += std::to_string(rcvr_id);

    MessageOut lc_out(lc_name_out.c_str());

    // Log base
#ifdef _ON_DEBUG_X
    SCOT_LOGALIGN_T* log_base = log->get_base();
    __SCOT_INFO__(lc_out, "{} spawned. Log base starts at {:x}.", 
        lc_name_out, uintptr_t(log_base));
#endif

    struct ScotMessageHeader* rcvd;

    while (1) {

        // Pause when signaled.
        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        // Wait for WAIT.
        rcvd = reinterpret_cast<struct ScotMessageHeader*>(
            log->poll_next_local_log(SCOT_MSGTYPE_WAIT)
        );
    
        











    }
}


void scot::ScotReceiver::worker_spawn() {
    worker = std::thread([&](){ __worker(&(this->log), this->id); });
    worker.detach();
}


void scot::ScotReceiver::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}