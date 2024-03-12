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
#include "./includes/scot-mout.hh"
#include "./includes/scot-conn.hh"
#include "./includes/scot-hb.hh"
#include "./includes/scot-log.hh"


// ScotConnContextPool
scot::ScotConnContextPool::ScotConnContextPool(int nid, int qsz_a, int psz_a)
    : local_nid(nid), quorum_sz(qsz_a), qp_pool_sz(psz_a) {

    assert(qp_pool_sz < SCOT_MAX_QPOOLSZ);
    ctx_pool.resize(qp_pool_sz);
}


std::vector<std::vector<struct scot::ConnContext>>& scot::ScotConnContextPool::get_pool_list() {
    return ctx_pool;
}


int scot::ScotConnContextPool::get_local_nid() { 
    return local_nid; 
}


int scot::ScotConnContextPool::get_quorum_sz() { 
    return quorum_sz; 
}


int scot::ScotConnContextPool::get_qp_pool_sz() { 
    return qp_pool_sz; 
}


std::vector<struct scot::ConnContext>* scot::ScotConnContextPool::pop_qlist() {

    int left; 
    while (!((left = allowed.fetch_sub(1)) > 0))
        allowed.fetch_add(1);   // Return if not allowed.

    // std::lock_guard<std::mutex> scoped_guard(pool_lock);
    while (pool_lock.test_and_set(std::memory_order_acquire))
        ;

    std::vector<struct scot::ConnContext>* next = ctx_free.front();
    ctx_free.pop();

    pool_lock.clear(std::memory_order_relaxed);

    return next;
}


void scot::ScotConnContextPool::push_qlist(std::vector<struct scot::ConnContext>* qlist) {

    while (pool_lock.test_and_set(std::memory_order_acquire))
        ;
    
    ctx_free.push(qlist);
    pool_lock.clear(std::memory_order_relaxed);

    allowed.fetch_add(1);
}


std::string scot::helper::key_generator(const char* resrc, int local, int remote, int qid) {
    
        std::string key(resrc);
        if (local >= 0) { 
            key += "-"; 
            key += std::to_string(local); 
        }

        if (remote >= 0) { 
            key += "-"; 
            key += std::to_string(remote); 
        }
        
        if (qid >= 0) { 
            key += "-"; 
            key += std::to_string(qid); 
        }

        return key;
    }


namespace helper {

    const char* rpli_ks[] = {HBKEY_MR_RPLI, HBKEY_QP_RPLI, HBKEY_SCQ_RPLI, HBKEY_RCQ_RPLI};
    const char* chkr_ks[] = {HBKEY_MR_CHKR, HBKEY_QP_CHKR, HBKEY_SCQ_CHKR, HBKEY_RCQ_CHKR};
    const char* rply_ks[] = {HBKEY_MR_RPLY, HBKEY_QP_RPLY, HBKEY_SCQ_RPLY, HBKEY_RCQ_RPLY};
    const char* rcvr_ks[] = {HBKEY_MR_RCVR, HBKEY_QP_RCVR, HBKEY_SCQ_RCVR, HBKEY_RCQ_RCVR};

    enum { KEY_MR = 0, KEY_QP, KEY_SCQ, KEY_RCQ };


    void init_pd(const char* pd_name) {
        hartebeest_create_local_pd(pd_name);
    }


    void prepare_writer_asset(
        scot::MessageOut& out,
        scot::ScotConnContextPool& writer_pool, const char** keyset) {

#ifdef __DEBUG__
        __SCOT_INFO__(out, "→ {} start.", __func__);
#endif

        // initializes replicator assets
        // 1. Make MR.
        hartebeest_create_local_mr(
            HBKEY_PD, scot::helper::key_generator(keyset[KEY_MR]).c_str(), SCOT_BUFFER_SZ,
            0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
        );

        int lnid = writer_pool.get_local_nid();

        //
        // Build pool
        for (int qpool_sid = 0; qpool_sid < writer_pool.get_qp_pool_sz(); qpool_sid++) {
            
            for (int rnid = 0; rnid < writer_pool.get_quorum_sz(); rnid++) {

                if (rnid == lnid) continue;

                // 1. Create SEND Queue
                hartebeest_create_basiccq(
                    scot::helper::key_generator(keyset[KEY_SCQ], lnid, rnid, qpool_sid).c_str());

                // 2. Create RECV Queue
                hartebeest_create_basiccq(
                    scot::helper::key_generator(keyset[KEY_RCQ], lnid, rnid, qpool_sid).c_str());

                // 3. Create Queue Pair
                hartebeest_create_local_qp(
                    HBKEY_PD, 
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    IBV_QPT_RC,
                    scot::helper::key_generator(keyset[KEY_SCQ], lnid, rnid, qpool_sid).c_str(),
                    scot::helper::key_generator(keyset[KEY_RCQ], lnid, rnid, qpool_sid).c_str()
                );

                hartebeest_init_local_qp(
                    HBKEY_PD,
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str()
                );

                // 4. Push to Memcached
                bool is_pushed = hartebeest_memc_push_local_qp(
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    HBKEY_PD,
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str()
                );
                assert(is_pushed == true);
            }
        }
    }


    void prepare_reader_asset(
        scot::MessageOut& out,
        scot::ScotConnContextPool& writer_pool, const char** keyset) {

#ifdef __DEBUG__
        __SCOT_INFO__(out, "→ {} start.", __func__);
#endif

        // initializes replicator assets
        int lnid = writer_pool.get_local_nid();
        
        size_t aligned_offset = (SCOT_BUFFER_SZ / writer_pool.get_qp_pool_sz());
        if (aligned_offset % sizeof(SCOT_LOGALIGN_T) != 0) {
            aligned_offset -= (aligned_offset % sizeof(SCOT_LOGALIGN_T));
        }

        for (int rnid = 0; rnid < writer_pool.get_quorum_sz(); rnid++) {

            if (rnid == lnid) continue;

            // 1. Make MR, One-per-remote, and push to Memcached
            hartebeest_create_local_mr(
                HBKEY_PD, scot::helper::key_generator(keyset[KEY_MR], lnid, rnid).c_str(), SCOT_BUFFER_SZ,
                0 | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
            );

            bool is_pushed = hartebeest_memc_push_local_mr(
                scot::helper::key_generator(keyset[KEY_MR], lnid, rnid).c_str(),
                HBKEY_PD,
                scot::helper::key_generator(keyset[KEY_MR], lnid, rnid).c_str()
            );

            // 2. For each remote pool ctx, that access this MR,
            for (int qpool_sid = 0; qpool_sid < writer_pool.get_qp_pool_sz(); qpool_sid++) {

                // 1. Create SEND Queue
                hartebeest_create_basiccq(
                    scot::helper::key_generator(keyset[KEY_SCQ], lnid, rnid, qpool_sid).c_str());

                // 2. Create RECV Queue
                hartebeest_create_basiccq(
                    scot::helper::key_generator(keyset[KEY_RCQ], lnid, rnid, qpool_sid).c_str());

                // 3. Create Queue Pair
                hartebeest_create_local_qp(
                    HBKEY_PD, 
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    IBV_QPT_RC,
                    scot::helper::key_generator(keyset[KEY_SCQ], lnid, rnid, qpool_sid).c_str(),
                    scot::helper::key_generator(keyset[KEY_RCQ], lnid, rnid, qpool_sid).c_str()
                );

                hartebeest_init_local_qp(
                    HBKEY_PD,
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str()
                );

                // 4. Push to Memcached
                is_pushed = hartebeest_memc_push_local_qp(
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    HBKEY_PD,
                    scot::helper::key_generator(keyset[KEY_QP], lnid, rnid, qpool_sid).c_str()
                );
                assert(is_pushed == true);


                // 5. Initialize local area.
                struct ScotAlignedLog* llog = 
                    reinterpret_cast<struct ScotAlignedLog*>(
                        hartebeest_get_local_mr(
                                HBKEY_PD, 
                                scot::helper::key_generator(keyset[KEY_MR], lnid, rnid).c_str()
                            )->addr);

                llog = reinterpret_cast<struct ScotAlignedLog*>(
                    uintptr_t(llog) + uintptr_t(qpool_sid * aligned_offset));
            
                llog->aligned = reinterpret_cast<SCOT_LOGALIGN_T*>(
                    uintptr_t(llog) + sizeof(struct ScotAlignedLog)
                );

                llog->bound = aligned_offset;
                llog->reserved[SCOT_RESERVED_IDX_FREE] = 4;

            }
        }
    }


    // Fetches remote assets
    void connect_rw_and_build_pool(
        scot::MessageOut& out,
        scot::ScotConnContextPool& writer_pool, const char** writer_keyset, const char** reader_keyset) {
        
        int lnid = writer_pool.get_local_nid();

        size_t aligned_offset = (SCOT_BUFFER_SZ / writer_pool.get_qp_pool_sz());
        if (aligned_offset % sizeof(SCOT_LOGALIGN_T) != 0) {
            aligned_offset -= (aligned_offset % sizeof(SCOT_LOGALIGN_T));
        }

#ifdef __DEBUG__
        __SCOT_INFO__(out, "→ {} start, build & connecting writers.", __func__);
#endif
        
        // For replicator module:
        int qpool_sid = 0;  // QP Pool Set ID
        for (auto& ctx_pool: writer_pool.get_pool_list()) {
            
            for (int rnid = 0; rnid < writer_pool.get_quorum_sz(); rnid++) {

                if (rnid == lnid) continue;
            
                scot::ConnContext ctx_elem(rnid);

                ctx_elem.qpool_sid = qpool_sid;
                ctx_elem.offset = qpool_sid * aligned_offset;

                // Initialize as a log area                
                // 1. Register Local MR/QP
                ctx_elem.local.mr = hartebeest_get_local_mr(
                    HBKEY_PD, scot::helper::key_generator(writer_keyset[KEY_MR]).c_str());

                struct ScotAlignedLog* log = reinterpret_cast<struct ScotAlignedLog*>(
                    uintptr_t(ctx_elem.local.mr->addr) + uintptr_t(ctx_elem.offset)
                );

                // 2. Local area initialize.
                log->aligned = reinterpret_cast<SCOT_LOGALIGN_T*>(
                    uintptr_t(ctx_elem.local.mr->addr) + uintptr_t(ctx_elem.offset) + sizeof(struct ScotAlignedLog)
                );

                // printf("%s %p aligned\n", __func__, log->aligned);

                log->bound = aligned_offset;
                log->reserved[SCOT_RESERVED_IDX_FREE] = 4; // Next?

                assert((reinterpret_cast<uintptr_t>(ctx_elem.local.mr) % sizeof(SCOT_LOGALIGN_T)) == 0);

                ctx_elem.local.qp = hartebeest_get_local_qp(
                    HBKEY_PD, scot::helper::key_generator(writer_keyset[KEY_QP], lnid, rnid, qpool_sid).c_str());

                // 3. Fetch Remote MR/QP Info from Memcached
                hartebeest_memc_fetch_remote_mr(
                    scot::helper::key_generator(reader_keyset[KEY_MR], rnid, lnid).c_str());

#ifdef __DEBUG__
                __SCOT_INFO__(out, "→ Fetched: {}, for {}", 
                    scot::helper::key_generator(reader_keyset[KEY_MR], rnid, lnid).c_str(), qpool_sid);
#endif

                hartebeest_memc_fetch_remote_qp(
                    scot::helper::key_generator(reader_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str());

#ifdef __DEBUG__
                __SCOT_INFO__(out, "→ Fetched: {}", 
                    scot::helper::key_generator(reader_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str());
#endif

                ctx_elem.remote.mr = hartebeest_get_remote_mr(
                    scot::helper::key_generator(reader_keyset[KEY_MR], rnid, lnid).c_str()
                );

                hartebeest_connect_local_qp(
                    HBKEY_PD, 
                    scot::helper::key_generator(writer_keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    scot::helper::key_generator(reader_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str());

                ctx_pool.push_back(std::move(ctx_elem));
            }

            qpool_sid++;
        }

        assert(writer_pool.get_pool_list().size() == writer_pool.get_qp_pool_sz());

#ifdef __DEBUG__
        __SCOT_INFO__(out, "→ Now, connecting readers.");
#endif

        // Connect receiver side
        for (int rnid = 0; rnid < writer_pool.get_quorum_sz(); rnid++) {

            if (rnid == lnid) continue;

            for (int qpool_sid = 0; qpool_sid < writer_pool.get_qp_pool_sz(); qpool_sid++) {

// #ifdef __DEBUG__
//         __SCOT_INFO__(out, "→ Waiting: {}", scot::helper::key_generator(writer_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str());
// #endif
                
                hartebeest_memc_fetch_remote_qp(
                    scot::helper::key_generator(writer_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str());

                hartebeest_connect_local_qp(
                    HBKEY_PD,
                    scot::helper::key_generator(reader_keyset[KEY_QP], lnid, rnid, qpool_sid).c_str(),
                    scot::helper::key_generator(writer_keyset[KEY_QP], rnid, lnid, qpool_sid).c_str()
                );
            }
        }

#ifdef __DEBUG__
        __SCOT_INFO__(out, "→ {} end.", __func__);
#endif

    }


    // Alert wait
    void notify_wait(
        scot::MessageOut& out,
        scot::ScotConnContextPool& rpli_pool) {
        
        // Notify others: "I AM FINISHED"
        hartebeest_memc_push_general(
            (std::to_string(rpli_pool.get_local_nid()) + "-wait").c_str());

        for (auto& ctx: rpli_pool.get_pool_list().at(0)) {
            hartebeest_memc_wait_general(
                (std::to_string(ctx.rnid) + "-wait").c_str()
            );
        }
    }
}


void scot::ScotConnection::__init_hartebeest() {
    
    // Initialize Hartebeest
    hartebeest_init();
    
    ::helper::init_pd(HBKEY_PD);

    assert(ctx_pool_rpli != nullptr);
    assert(ctx_pool_chkr != nullptr);
    assert(ctx_pool_drmr != nullptr);

    // helper::prepare_rpli_asset(*ctx_pool_rpli);
    // helper::prepare_rply_asset(*ctx_pool_rpli);

    // Create RDMA resource
    ::helper::prepare_writer_asset(out, *ctx_pool_rpli, ::helper::rpli_ks);
    ::helper::prepare_reader_asset(out, *ctx_pool_rpli, ::helper::rply_ks);

    // Prepare free queue
    for (auto& qlist: ctx_pool_rpli->get_pool_list())
        ctx_pool_rpli->push_qlist(&qlist);

    ::helper::prepare_writer_asset(out, *ctx_pool_chkr, ::helper::chkr_ks);
    ::helper::prepare_reader_asset(out, *ctx_pool_chkr, ::helper::rcvr_ks);

    for (auto& qlist: ctx_pool_chkr->get_pool_list())
        ctx_pool_chkr->push_qlist(&qlist);

    ::helper::connect_rw_and_build_pool(
        out, *ctx_pool_rpli, ::helper::rpli_ks, ::helper::rply_ks);

    ::helper::connect_rw_and_build_pool(
        out, *ctx_pool_chkr, ::helper::chkr_ks, ::helper::rcvr_ks);

    ::helper::notify_wait(out, *ctx_pool_rpli);

}


void scot::ScotConnection::__finalize_hartebeest() {

}


scot::ScotConnection::ScotConnection() 
    : ctx_pool_rpli(nullptr), ctx_pool_chkr(nullptr), ctx_pool_drmr(nullptr), out("conn") {

    // Configure Quorum
    nid = std::stoi(
        std::string(hartebeest_get_sysvar(SCOT_ENVVAR_NID))
    );

    int quorum_sz = std::stoi(
        std::string(getenv(SCOT_ENVVAR_QSZ))
    );

    qsz = quorum_sz;

    int qp_pool_sz = std::stoi(
        std::string(getenv(SCOT_ENVVAR_QPOOLSZ))
    );

    assert(qp_pool_sz < SCOT_MAX_QPOOLSZ);

    ctx_pool_rpli = new ScotConnContextPool(nid, quorum_sz, qp_pool_sz);
    ctx_pool_chkr = new ScotConnContextPool(nid, quorum_sz, qp_pool_sz);
    ctx_pool_drmr = new ScotConnContextPool(nid, quorum_sz, qp_pool_sz);

    __init_hartebeest();
}


scot::ScotConnection::~ScotConnection() {

    if (ctx_pool_rpli != nullptr)
        delete ctx_pool_rpli;

    if (ctx_pool_chkr != nullptr)
        delete ctx_pool_chkr;

    if (ctx_pool_drmr != nullptr)
        delete ctx_pool_drmr;

    __finalize_hartebeest();
}


int scot::ScotConnection::get_my_nid() {
    return nid;
}


int scot::ScotConnection::get_quorum_sz() {
    return qsz;
}



void scot::ScotConnection::start() {

}


void scot::ScotConnection::end() {

}


scot::ScotConnContextPool* scot::ScotConnection::get_rpli_ctx_pool() {
    return ctx_pool_rpli;
}


scot::ScotConnContextPool* scot::ScotConnection::get_chkr_ctx_pool() {
    return ctx_pool_chkr;
}


scot::ScotConnContextPool* scot::ScotConnection::get_drmr_ctx_pool() {
    return ctx_pool_drmr;
}

