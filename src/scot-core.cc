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


scot::ScotCore::ScotCore(SCOT_USRACTION ext_func) : msg_out("scot-core") {

    __SCOT_INFO__(msg_out, "Core init start.");

    // Initialize connection
    ScotConnection::get_instance().start();

    nid = ScotConnection::get_instance().get_my_nid();
    qsize = ScotConnection::get_instance().get_quorum_sz();
    qpool_size = ScotConnection::get_instance().get_rpli_ctx_pool()->get_qp_pool_sz();

    // Configuration value settings
    uint64_t prefetch_size = ldr.get_confval("prefetch-size");

    ext_confvar.insert(
        std::pair<std::string, uint64_t>(std::string("inline-max"), ldr.get_confval("inline-max"))
    );

    ext_confvar.insert(
        std::pair<std::string, uint64_t>(std::string("prefetch-size"), prefetch_size)
    );

    
    rpli = new ScotReplicator(ScotConnection::get_instance().get_rpli_ctx_pool());
    __SCOT_INFO__(msg_out, "→ A Replicator initiated: 0x{:x}", uintptr_t(rpli));

    chkr = new ScotChecker(ScotConnection::get_instance().get_chkr_ctx_pool());
    __SCOT_INFO__(msg_out, "→ A Checker initiated: 0x{:x}", uintptr_t(chkr));

    struct ibv_mr* mr = nullptr;    
    for (auto& ctx_pool: 
        ScotConnection::get_instance().get_rpli_ctx_pool()->get_pool_list().at(0)) {
        
        mr = hartebeest_get_local_mr(HBKEY_PD, 
            scot::helper::key_generator(HBKEY_MR_RPLY, nid, ctx_pool.rnid).c_str());
        vec_rply.push_back(
            new ScotReplayer(
                ctx_pool.rnid, 
                reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr), 
                ScotConnection::get_instance().get_rpli_ctx_pool(),
                chkr, 
                ext_func)
        );

        __SCOT_INFO__(msg_out, "→ Replayer({}) added, MR base(0x{:x})", ctx_pool.rnid, uintptr_t(mr->addr));

        vec_rply.back()->distr_spawn();
    }

    for (auto& ctx_pool: 
        ScotConnection::get_instance().get_chkr_ctx_pool()->get_pool_list().at(0)) {

        mr = hartebeest_get_local_mr(HBKEY_PD, 
            scot::helper::key_generator(HBKEY_MR_RCVR, nid, ctx_pool.rnid).c_str());
        vec_rcvr.push_back(
            new ScotReceiver(
                ctx_pool.rnid, 
                reinterpret_cast<SCOT_LOGALIGN_T*>(mr->addr),
                ScotConnection::get_instance().get_chkr_ctx_pool(),
                rpli,
                ext_func
            )
        );

        __SCOT_INFO__(msg_out, "→ Receiver({}) added.", ctx_pool.rnid);

        vec_rcvr.back()->worker_spawn();
    }
    
    if (ext_func != nullptr)
        __SCOT_INFO__(msg_out, "→ Replay function registered.");

    for (auto& rply: vec_rply) {
        rply->distr_signal_toggle(SCOT_WRKR_PAUSE); // Disable pause
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


uint32_t scot::ScotCore::hash(char* buf, int buf_len) {
    return rpli->hash(buf, buf_len);
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