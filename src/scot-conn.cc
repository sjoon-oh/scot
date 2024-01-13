/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <cassert>

#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-conn.hh"
#include "./includes/scot-hb.hh"


// Keysets
namespace scot {
    const char* rpli_ks[] = {HBKEY_MR_RPLI, HBKEY_QP_RPLI, HBKEY_SCQ_RPLI, HBKEY_RCQ_RPLI};
    const char* chkr_ks[] = {HBKEY_MR_CHKR, HBKEY_QP_CHKR, HBKEY_SCQ_CHKR, HBKEY_RCQ_CHKR};
    const char* rply_ks[] = {HBKEY_MR_RPLY, HBKEY_QP_RPLY, HBKEY_SCQ_RPLY, HBKEY_RCQ_RPLY};
    const char* rcvr_ks[] = {HBKEY_MR_RCVR, HBKEY_QP_RCVR, HBKEY_SCQ_RCVR, HBKEY_RCQ_RCVR};
}


std::string scot::wkey_helper(const char* resrc, int local, int remote) {
    
    std::string key(resrc);
    if (local >= 0) { key += "-"; key += std::to_string(local); }
    if (remote >= 0) { key += "-"; key += std::to_string(remote); }

    return key;
}


std::string scot::rkey_helper(const char* resrc, int local, int remote) {

    std::string key(resrc);
    if (remote >= 0) { key += "-"; key += std::to_string(remote); }
    if (local >= 0) { key += "-"; key += std::to_string(local); }
    
    return key;
}


void scot::ScotConnection::__init_hartebeest() {
    
    // Initialize Hartebeest
    hartebeest_init();
    hartebeest_create_local_pd(HBKEY_PD);
    
    // Initialize Writers MR
    hartebeest_create_local_mr(
        HBKEY_PD, wkey_helper(rpli_ks[KEY_MR]).c_str(), SCOT_BUFFER_SZ,
        0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
    );

    hartebeest_create_local_mr(
        HBKEY_PD, wkey_helper(chkr_ks[KEY_MR]).c_str(), SCOT_BUFFER_SZ,
        0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
    );

    // Heartbeat scoreboard
    hartebeest_create_local_mr(
        HBKEY_PD, wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str(), sizeof(struct ScotScoreboard),
        0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
    );

    hartebeest_memc_push_local_mr(
        wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str(), 
        HBKEY_PD, wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str());

    struct ibv_mr* rpli_mr = hartebeest_get_local_mr(HBKEY_PD, wkey_helper(rpli_ks[KEY_MR]).c_str());
    struct ibv_mr* chkr_mr = hartebeest_get_local_mr(HBKEY_PD, wkey_helper(chkr_ks[KEY_MR]).c_str());
    struct ibv_mr* hbtr_mr = hartebeest_get_local_mr(HBKEY_PD, wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str());

    for (auto& ctx: quroum_conns) {
    
#define __KEY_MR__(SET, HELPER)     (HELPER(SET[KEY_MR], std::stoi(nid), ctx.nid).c_str())
#define __KEY_QP__(SET, HELPER)     (HELPER(SET[KEY_QP], std::stoi(nid), ctx.nid).c_str())
#define __KEY_SCQ__(SET, HELPER)    (HELPER(SET[KEY_SCQ], std::stoi(nid), ctx.nid).c_str())
#define __KEY_RCQ__(SET, HELPER)    (HELPER(SET[KEY_RCQ], std::stoi(nid), ctx.nid).c_str())

        //
        // Start Writers QP
        hartebeest_create_basiccq(__KEY_SCQ__(rpli_ks, wkey_helper));
        hartebeest_create_basiccq(__KEY_RCQ__(rpli_ks, wkey_helper));
        hartebeest_create_basiccq(__KEY_SCQ__(chkr_ks, wkey_helper));
        hartebeest_create_basiccq(__KEY_RCQ__(chkr_ks, wkey_helper));

        hartebeest_create_local_qp(
            HBKEY_PD, __KEY_QP__(rpli_ks, wkey_helper), IBV_QPT_RC, 
            __KEY_SCQ__(rpli_ks, wkey_helper),__KEY_RCQ__(rpli_ks, wkey_helper));
        hartebeest_create_local_qp(
            HBKEY_PD, __KEY_QP__(chkr_ks, wkey_helper), IBV_QPT_RC, 
            __KEY_SCQ__(chkr_ks, wkey_helper),__KEY_RCQ__(chkr_ks, wkey_helper));

        hartebeest_init_local_qp(HBKEY_PD, __KEY_QP__(rpli_ks, wkey_helper));
        hartebeest_init_local_qp(HBKEY_PD, __KEY_QP__(chkr_ks, wkey_helper));

        ctx.local.rpli_qp = hartebeest_get_local_qp(HBKEY_PD, __KEY_QP__(rpli_ks, wkey_helper));
        ctx.local.chkr_qp = hartebeest_get_local_qp(HBKEY_PD, __KEY_QP__(chkr_ks, wkey_helper));

        ctx.local.rpli_mr = rpli_mr;
        ctx.local.chkr_mr = chkr_mr;

        bool is_pushed = hartebeest_memc_push_local_qp(
            __KEY_QP__(rpli_ks, wkey_helper), HBKEY_PD, __KEY_QP__(rpli_ks, wkey_helper));
        assert(is_pushed == true);

        is_pushed = hartebeest_memc_push_local_qp(
            __KEY_QP__(chkr_ks, wkey_helper), HBKEY_PD, __KEY_QP__(chkr_ks, wkey_helper));
        assert(is_pushed == true);

        //
        // Start Receiver QP
        hartebeest_create_basiccq(__KEY_SCQ__(rply_ks, rkey_helper));
        hartebeest_create_basiccq(__KEY_RCQ__(rply_ks, rkey_helper));
        hartebeest_create_basiccq(__KEY_SCQ__(rcvr_ks, rkey_helper));
        hartebeest_create_basiccq(__KEY_RCQ__(rcvr_ks, rkey_helper));

        hartebeest_create_local_mr(
            HBKEY_PD, __KEY_MR__(rply_ks, rkey_helper), SCOT_BUFFER_SZ,
            0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
        );
        hartebeest_create_local_mr(
            HBKEY_PD, __KEY_MR__(rcvr_ks, rkey_helper), SCOT_BUFFER_SZ,
            0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
        );

        hartebeest_create_local_qp(
            HBKEY_PD, __KEY_QP__(rply_ks, rkey_helper), IBV_QPT_RC, 
            __KEY_SCQ__(rply_ks, rkey_helper),__KEY_RCQ__(rply_ks, rkey_helper));
        hartebeest_create_local_qp(
            HBKEY_PD, __KEY_QP__(rcvr_ks, rkey_helper), IBV_QPT_RC, 
            __KEY_SCQ__(rcvr_ks, rkey_helper),__KEY_RCQ__(rcvr_ks, rkey_helper));

        hartebeest_init_local_qp(HBKEY_PD, __KEY_QP__(rply_ks, rkey_helper));
        hartebeest_init_local_qp(HBKEY_PD, __KEY_QP__(rcvr_ks, rkey_helper));

        is_pushed = hartebeest_memc_push_local_qp(
            __KEY_QP__(rply_ks, rkey_helper), HBKEY_PD, __KEY_QP__(rply_ks, rkey_helper));
        assert(is_pushed == true);
        is_pushed = hartebeest_memc_push_local_mr(
            __KEY_MR__(rply_ks, rkey_helper), HBKEY_PD, __KEY_MR__(rply_ks, rkey_helper));
        assert(is_pushed == true);

        is_pushed = hartebeest_memc_push_local_qp(
            __KEY_QP__(rcvr_ks, rkey_helper), HBKEY_PD, __KEY_QP__(rcvr_ks, rkey_helper));
        assert(is_pushed == true);
        is_pushed = hartebeest_memc_push_local_mr(
            __KEY_MR__(rcvr_ks, rkey_helper), HBKEY_PD, __KEY_MR__(rcvr_ks, rkey_helper));
        assert(is_pushed == true);

// Heartbeat-related
#define __KEY_QP_HB_LOCAL__         (wkey_helper(HBKEY_QP_HBTR, std::stoi(nid), ctx.nid).c_str())
#define __KEY_QP_HB_REMOTE__        (rkey_helper(HBKEY_QP_HBTR, std::stoi(nid), ctx.nid).c_str())
#define __KEY_SCQ_HB_LOCAL__        (wkey_helper(HBKEY_SCQ_HBTR, std::stoi(nid), ctx.nid).c_str())
#define __KEY_RCQ_HB_LOCAL__        (wkey_helper(HBKEY_RCQ_HBTR, std::stoi(nid), ctx.nid).c_str())
        
        //
        // Start Hartbeat QPs
        hartebeest_create_basiccq(__KEY_SCQ_HB_LOCAL__);
        hartebeest_create_basiccq(__KEY_RCQ_HB_LOCAL__);

        hartebeest_create_local_qp(
            HBKEY_PD, __KEY_QP_HB_LOCAL__, IBV_QPT_RC, __KEY_SCQ_HB_LOCAL__, __KEY_RCQ_HB_LOCAL__);
        
        hartebeest_init_local_qp(HBKEY_PD, __KEY_QP_HB_LOCAL__);
        is_pushed = hartebeest_memc_push_local_qp(
            __KEY_QP_HB_LOCAL__, HBKEY_PD, __KEY_QP_HB_LOCAL__);
        assert(is_pushed == true);

        ctx.local.hbtr_mr = hbtr_mr;
        ctx.local.hbtr_qp = hartebeest_get_local_qp(HBKEY_PD, __KEY_QP_HB_LOCAL__);
    } 
}


void scot::ScotConnection::__finalize_hartebeest() {
    
    for (auto& ctx: quroum_conns) {
        hartebeest_memc_del_general(__KEY_QP__(rpli_ks, wkey_helper));
        hartebeest_memc_del_general(__KEY_QP__(chkr_ks, wkey_helper));

        hartebeest_memc_del_general(__KEY_QP__(rply_ks, rkey_helper));
        hartebeest_memc_del_general(__KEY_QP__(rcvr_ks, rkey_helper));

        hartebeest_memc_del_general(__KEY_MR__(rply_ks, rkey_helper));
        hartebeest_memc_del_general(__KEY_MR__(rcvr_ks, rkey_helper));
    }
}


void scot::ScotConnection::__connect_qps() {
    
    for (auto& ctx: quroum_conns) {
        
        // For writers : Replicator & Remote Replayers, Checker & Remote Receivers
        hartebeest_memc_fetch_remote_mr(__KEY_MR__(rply_ks, wkey_helper));  // Me writing to Remote.
        hartebeest_memc_fetch_remote_qp(__KEY_QP__(rply_ks, wkey_helper));  // Me writing to Remote.
        ctx.remote.rply_mr = hartebeest_get_remote_mr(__KEY_MR__(rply_ks, wkey_helper));
        
        hartebeest_connect_local_qp(
            HBKEY_PD, 
            __KEY_QP__(rpli_ks, wkey_helper),
            __KEY_QP__(rply_ks, wkey_helper)
        );

        hartebeest_memc_fetch_remote_mr(__KEY_MR__(rcvr_ks, wkey_helper));  // Me writing to Remote.
        hartebeest_memc_fetch_remote_qp(__KEY_QP__(rcvr_ks, wkey_helper));  // Me writing to Remote.
        ctx.remote.rcvr_mr = hartebeest_get_remote_mr(__KEY_MR__(rcvr_ks, wkey_helper));
        
        hartebeest_connect_local_qp(
            HBKEY_PD, 
            __KEY_QP__(chkr_ks, wkey_helper),
            __KEY_QP__(rcvr_ks, wkey_helper)
        );

        // For Readers : Local Replayers
        hartebeest_memc_fetch_remote_qp(__KEY_QP__(rpli_ks, rkey_helper));  // Me being wrtten from Remote.
        hartebeest_connect_local_qp(
            HBKEY_PD, 
            __KEY_QP__(rply_ks, rkey_helper),
            __KEY_QP__(rpli_ks, rkey_helper)
        );

        hartebeest_memc_fetch_remote_qp(__KEY_QP__(chkr_ks, rkey_helper));  // Me being wrtten from Remote.
        hartebeest_connect_local_qp(
            HBKEY_PD, 
            __KEY_QP__(rcvr_ks, rkey_helper),
            __KEY_QP__(chkr_ks, rkey_helper)
        );

        // For Hearbeat:
        hartebeest_memc_fetch_remote_mr(
            wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str()
        );
        hartebeest_memc_fetch_remote_qp(__KEY_QP_HB_REMOTE__);
        hartebeest_connect_local_qp(
            HBKEY_PD, 
            __KEY_QP_HB_LOCAL__,
            __KEY_QP_HB_REMOTE__
        );

        ctx.remote.hbtr_mr = hartebeest_get_remote_mr(
            wkey_helper(HBKEY_MR_HBTR, std::stoi(nid)).c_str()
        );

    }
}


void scot::ScotConnection::__wait_connection() {

    hartebeest_memc_push_general((nid + "-wait").c_str());

    for (auto& ctx: quroum_conns) {
        hartebeest_memc_wait_general((std::to_string(ctx.nid) + "-wait").c_str());
    }
}


scot::ScotConnection::ScotConnection() {

    // Configure Quorum
    nid = hartebeest_get_sysvar(SCOT_ENVVAR_NID);
    quorum_sz = std::stoi(
        std::string(getenv(SCOT_ENVVAR_QSZ))
    );

    // Record all ids
    for (int id = 0; id < quorum_sz; id++) {
        if (id != get_my_nid())
            quroum_conns.push_back(ConnContext(id));
    }
    
    __init_hartebeest();
    {
        __connect_qps();
        __wait_connection();
    }
    __finalize_hartebeest();
}


int scot::ScotConnection::get_my_nid() {
    return std::stoi(nid);
}


int scot::ScotConnection::get_quorum_sz() {
    return quorum_sz;
}

std::vector<struct scot::ConnContext>& scot::ScotConnection::get_quorum() {
    return quroum_conns;
}

void scot::ScotConnection::start() {

}

void scot::ScotConnection::end() {
    
}