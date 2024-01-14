/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include "../../extern/nlohmann/json.hpp"
#include "../../hartebeest/src/includes/hartebeest-c.h"

#include "../../src/includes/scot-def.hh"
#include "./includes/menc-core.hh"


scot_menc::ScotMenciusCore::ScotMenciusCore() 
    : msg_out("scot-menc-core"), rc_inline_max(0) {

    __SCOT_INFO__(msg_out, "Core init start.");

    // Initialize connection
    scot::ScotConnection::get_instance().start();

    nid = scot::ScotConnection::get_instance().get_my_nid();
    qsize = scot::ScotConnection::get_instance().get_quorum_sz();

    rc_inline_max = ldr.get_confval("inline-max");
    menc_skip_delay = ldr.get_confval("mencius-skip-delay");

    rpli = new ScotMenciusReplicator(
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, scot::wkey_helper(HBKEY_MR_RPLI).c_str())->addr), nid
    );
    
    __SCOT_INFO__(msg_out, "→ A Replicator initiated: 0x{:x}", uintptr_t(rpli));

    uint32_t active = ((nid + 1) % qsize);
    bool activate = 0;
    
    struct ibv_mr* mr = nullptr;
    for (auto& ctx: scot::ScotConnection::get_instance().get_quorum()) {
        
        activate = (uint32_t(ctx.nid) == active) ? true : false;

        mr = hartebeest_get_local_mr(HBKEY_PD, scot::rkey_helper(HBKEY_MR_RPLY, nid, ctx.nid).c_str());
        vec_rply.push_back(
            new ScotMenciusReplayer(
                reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid, activate, menc_skip_delay, rpli)
        );

        vec_rply.back()->worker_spawn();
    }

    __SCOT_INFO__(msg_out, "→ All Replayers spawned.");

    for (auto& rply: vec_rply) {
        rply->worker_signal_toggle(SCOT_WRKR_PAUSE); // Disable pause
    }
}


scot_menc::ScotMenciusCore::~ScotMenciusCore() {
    if (rpli != nullptr) {
        delete rpli;
        rpli = nullptr;
    }

    for (auto& rply: vec_rply) 
        if (rply != nullptr) delete rply;
}


void scot_menc::ScotMenciusCore::initialize() { }
void scot_menc::ScotMenciusCore::finish() { 

};


int scot_menc::ScotMenciusCore::propose(
    uint8_t* buf, uint16_t buf_sz, uint8_t* key, uint16_t key_sz, 
    uint32_t hashv) {
    
    rpli->write_request(
        buf, buf_sz, key, key_sz, hashv, SCOT_MSGTYPE_PURE
    );

    return SCOT_MSGTYPE_PURE;
}


uint32_t scot_menc::ScotMenciusCore::get_nid() {
    return nid;
}


uint32_t scot_menc::ScotMenciusCore::get_qsize() {
    return qsize;
}

