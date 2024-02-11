/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <regex>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"

// Configuration
scot::ScotConfLoader::ScotConfLoader() : msg_out("scot-ldr") {

    __SCOT_INFO__(msg_out, "Loading configuration.");

    const char* path = getenv(SCOT_CONFPATH); 
    
    nlohmann::json configuration;
    std::ifstream cf(path);

    if (cf.fail())
        assert(false);

    cf >> configuration;

    for (auto& elem: configuration.items()) {
        confs.insert(
            std::pair<std::string, uint64_t>(std::string(elem.key()), elem.value())
        );
        __SCOT_INFO__(msg_out, "Configuration set(SCOT): [{}, {}].", std::string(elem.key()), confs.at(std::string(elem.key())));
    }
}


uint64_t scot::ScotConfLoader::get_confval(const char* key) {
    if (confs.find(std::string(key)) == confs.end()) {
        assert(false);
        return 0;
    }
    else
        return confs.at(std::string(key));
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


scot::ScotCore::ScotCore() : msg_out("scot-core"), rc_inline_max(0) {

    __SCOT_INFO__(msg_out, "Core init start.");

    // Initialize connection
    ScotConnection::get_instance().start();

    nid = ScotConnection::get_instance().get_my_nid();
    qsize = ScotConnection::get_instance().get_quorum_sz();

    // Configuration value settings
    rc_inline_max = ldr.get_confval("inline-max");
    prefetch_size = ldr.get_confval("prefetch-size");

    rpli = new ScotReplicator(
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                hartebeest_get_local_mr(
                    HBKEY_PD, wkey_helper(HBKEY_MR_RPLI).c_str())->addr),
            prefetch_size
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
            new ScotReplayer(
                reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid, chkr)
        );

        vec_rply.back()->worker_spawn();
    }

    __SCOT_INFO__(msg_out, "→ All Replayers spawned.");

        for (auto& ctx: ScotConnection::get_instance().get_quorum()) {
        
        mr = hartebeest_get_local_mr(HBKEY_PD, rkey_helper(HBKEY_MR_RCVR, nid, ctx.nid).c_str());
        vec_rcvr.push_back(
            new ScotReceiver(
                reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), ctx.nid, rpli)
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
    uint32_t hashv) {

    struct ScotOwnership ownership;
    uint_judge.judge(&ownership, hashv);

#ifdef __DEBUG__
    __SCOT_INFO__(msg_out, "→ Proposing {}, owner({}).", hashv, ownership.owner);
#endif

    if (ownership.owner == nid) {
        rpli->write_request(
            buf, buf_sz, key, key_sz, hashv, SCOT_MSGTYPE_PURE
        );
        return SCOT_MSGTYPE_PURE;
    }
    else {
        chkr->write_request(
            buf, buf_sz, key, key_sz, hashv, ownership.owner
        );
        return SCOT_MSGTYPE_WAIT;
    }

    return SCOT_MSGTYPE_NONE;
};


void scot::ScotCore::initialize() { };
void scot::ScotCore::finish() { 

};


void scot::ScotCore::add_rule(uint32_t key, SCOT_RULEF_T rulef) {
    uint_judge.register_rule(key, rulef);
}


void scot::ScotCore::update_active(uint32_t key) {
    uint_judge.update_active(key);
}


uint32_t scot::ScotCore::get_nid() {
    return nid;
}


uint32_t scot::ScotCore::get_qsize() {
    return qsize;
}