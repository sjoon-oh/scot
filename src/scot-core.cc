
#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"

// Configuration
scot::ScotConfLoader::ScotConfLoader() {

}


scot::ScotWriter::ScotWriter(SCOT_LOGALIGN_T* addr) : log(addr) {
    writer_lock.clear();
}


SCOT_LOGALIGN_T* scot::ScotWriter::get_base() {
    return log.get_base();
}


scot::ScotReader::ScotReader(
        SCOT_LOGALIGN_T* addr) 
    : log(addr) {

}


scot::ScotCore::ScotCore() : msg_out("scot-core") {

    __SCOT_INFO__(msg_out, "Core init start.");

    // Initialize connection
    ScotConnection::get_instance().start();

    int nid = ScotConnection::get_instance().get_my_nid();
    // int qsize = ScotConnection::get_instance().get_quorum_sz();

    rpli = new ScotReplicator(
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, wkey_helper(HBKEY_MR_RPLI).c_str())->addr)
    );
    
    __SCOT_INFO__(msg_out, "→ A Replicator initiated: 0x{:x}", uintptr_t(rpli));

    chkr = new ScotChecker(
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, wkey_helper(HBKEY_MR_CHKR).c_str())->addr)
    );

    __SCOT_INFO__(msg_out, "→ A Checker initiated: 0x{:x}", uintptr_t(chkr));
    
    struct ibv_mr* mr = nullptr;
    for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
        
        mr = hartebeest_get_local_mr(HBKEY_PD, rkey_helper(HBKEY_MR_RPLY, nid, ctx.nid).c_str());
        vec_rply.push_back(
            new ScotReplayer(reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid)
        );

        vec_rply.back()->worker_spawn();
    }

    __SCOT_INFO__(msg_out, "→ All Replayers spawned.");

        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
        
        mr = hartebeest_get_local_mr(HBKEY_PD, rkey_helper(HBKEY_MR_RCVR, nid, ctx.nid).c_str());
        vec_rcvr.push_back(
            new ScotReceiver(reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid)
        );

        vec_rcvr.back()->worker_spawn();
    }

    __SCOT_INFO__(msg_out, "→ All Receivers spawned.");

    for (auto& rply: vec_rply) {
        rply->worker_signal_toggle(SCOT_WRKR_PAUSE); // Disable pause
    }

    for (auto& rcvr: vec_rcvr) {
        rcvr->worker_signal_toggle(SCOT_WRKR_PAUSE); // Disable pause
    }

}

scot::ScotCore::~ScotCore() {
    if (rpli != nullptr) {
        delete rpli;
        rpli = nullptr;
    }
    if (chkr != nullptr) {
        delete chkr;
        chkr = nullptr;
    }

    for (auto& rply: vec_rply) 
        if (rply != nullptr) delete rply;
}


int scot::ScotCore::propose(
    uint8_t* buf, uint16_t buf_sz, uint8_t* key, uint16_t key_sz, 
    uint32_t hashv, uint8_t msg) {
    
    //
    //
    // Rules here?
#ifdef _ON_DEBUG_X
    __SCOT_INFO__(msg_out, "→ Propose called.");
#endif



    // chkr->write_request(
    //     buf, buf_sz, key, key_sz, hashv, SCOT_MSGTYPE_WAIT
    // );

    // rpli->write_request(
    //     buf, buf_sz, key, key_sz, hashv, msg
    // );

    return 0;
};


void scot::ScotCore::initialize() { };
void scot::ScotCore::finish() { 

};