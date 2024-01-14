/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <iostream>

#include "../../hartebeest/src/includes/hartebeest-c.h"
#include "../../src/includes/scot-def.hh"
#include "./includes/menc-core.hh"


#define SCOT_MENC_SKIP_WAIT         7000
#define SCOT_MENC_SKIPPER_SKIP      0x01
#define SCOT_MENC_SKIPPER_IGNR      0x02

uint64_t SCOT_MENC_SKIPCNT = 0;


inline uint8_t do_skip(scot_menc::ScotMenciusReplicator* rpli, uint32_t nst) {

    struct timespec start, limit;
    long long elapsed = 0;

    SCOT_LOG_FINEGRAINED_T dummy_buf[3] = { 'A', 'B', 'C' };

    rpli->allow_write();
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {

        clock_gettime(CLOCK_MONOTONIC, &limit);
        elapsed = __ELAPSED_NSEC__(start, limit);

        if (elapsed > nst)
            break;
    }

    // If there is something on going, 
    //  test_and_set() will return true, as the lock is held.
    if (rpli->try_write() == 0) {
        rpli->write_request(dummy_buf, 3, dummy_buf, 1, 0, SCOT_MSGTYPE_HDRONLY);

        return SCOT_MENC_SKIPPER_SKIP;
    }

    return SCOT_MENC_SKIPPER_IGNR;
}


scot_menc::ScotMenciusReplayer::ScotMenciusReplayer(
    SCOT_LOGALIGN_T* addr, uint32_t rply_id, 
    bool activate, uint32_t delay, scot_menc::ScotMenciusReplicator* rpli_ul) 
        : scot::ScotReader(addr), worker(), worker_signal(0), id(rply_id), 
            skip_active(activate), menc_skip_delay(delay), rpli(rpli_ul) { 

    worker_signal_toggle(SCOT_WRKR_PAUSE);
}


scot_menc::ScotMenciusReplayer::~ScotMenciusReplayer() {
    worker_signal_toggle(SCOT_WRKR_HALT);
    std::cout << "SKIP counts: " << SCOT_MENC_SKIPCNT << std::endl;
}


#define IS_SIGNALED(SIGN)   (worker_signal & SIGN)
void scot_menc::ScotMenciusReplayer::__worker(
    scot::ScotLog* log, uint32_t rply_id, bool active, uint32_t delay, scot_menc::ScotMenciusReplicator* rpli) {

    const bool skip_active = active;

    std::string lc_name_out("menc-rply-");
    lc_name_out += std::to_string(rply_id);

    scot::MessageOut lc_out(lc_name_out.c_str());

    // Log base
#ifdef __DEBUG__
    SCOT_LOGALIGN_T* log_base = log->get_base();
    __SCOT_INFO__(lc_out, "{} spawned. Log base starts at {:x}.", 
        lc_name_out, uintptr_t(log_base));
#endif

    if (skip_active) {
#ifdef __DEBUG__        
        __SCOT_INFO__(lc_out, "{} spawned. I am the unlocker. ", lc_name_out); 
#endif
    }

    struct ScotMessageHeader* rcvd;
    SCOT_LOG_FINEGRAINED_T* pyld;

    while (1) {

        // Pause when signaled.
        while (IS_SIGNALED(SCOT_WRKR_PAUSE)) ;

        // End point, whether it is pure, or ACKed.
        rcvd = reinterpret_cast<struct ScotMessageHeader*>(
            log->poll_next_local_log(SCOT_MSGTYPE_PURE | SCOT_MSGTYPE_HDRONLY)
        );

        if (skip_active) {
            if (do_skip(rpli, delay) == SCOT_MENC_SKIPPER_SKIP) {
#ifdef __DEBUG__
                __SCOT_INFO__(lc_out, "→ Sending skip messages.");
#endif
            }
            else {
#ifdef __DEBUG__
                __SCOT_INFO__(lc_out, "→ No skips.");
            
#endif
            }
        }

        if (rcvd->msg & SCOT_MSGTYPE_COMMPREV) {
                
            // Replay function goes here.

#ifdef __DEBUG__
                __SCOT_INFO__(lc_out, "→ Commit piggybacked. Safe to replay.");
#endif
        }

        if (rcvd->msg & SCOT_MSGTYPE_HDRONLY) {
            SCOT_MENC_SKIPCNT++;
        }

        rcvd->msg = SCOT_MSGTYPE_NONE;
    }
}


void scot_menc::ScotMenciusReplayer::worker_spawn() {
    worker = std::thread([&](){ __worker(&(this->log), this->id, this->skip_active, this->menc_skip_delay, this->rpli); });
    worker.detach();
}


void scot_menc::ScotMenciusReplayer::worker_signal_toggle(uint32_t signal) {
    worker_signal ^= signal;
}



