#pragma once

/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <map>
#include <string>
#include <vector>
#include <queue>

#include <mutex>
#include <atomic>

#include <infiniband/verbs.h>

#include "./scot-mout.hh"

namespace scot {

    struct ConnContext {
        int rnid;
        int qpool_sid;
        size_t offset;

        struct  {
            struct ibv_qp* qp;     // Local connected QP
            struct ibv_mr* mr;
        } local;

        struct {
            struct ibv_mr* mr;     // Local connected QP
        } remote;
        
        ConnContext(int nid) : rnid(nid), qpool_sid(0), offset(0) { 
            local = {nullptr, nullptr};
            remote = {nullptr};
        }
    };


    namespace helper {
        std::string key_generator(const char*, int = -1, int = -1, int = -1);
    }


    class ScotConnContextPool {
    private:
        int local_nid;
        int quorum_sz;
        int qp_pool_sz;

        std::atomic_flag pool_lock = ATOMIC_FLAG_INIT;
        std::atomic<int32_t> allowed;

        std::vector<std::vector<struct ConnContext>> ctx_pool;
        std::queue<std::vector<struct ConnContext>*> ctx_free;

    public:
        ScotConnContextPool(int, int, int);
        ~ScotConnContextPool() = default;

        std::vector<std::vector<struct ConnContext>>& get_pool_list();

        int get_local_nid();
        int get_quorum_sz();
        int get_qp_pool_sz();

        std::vector<struct ConnContext>* pop_qlist();
        void push_qlist(std::vector<struct ConnContext>*);
    };


    class ScotConnection {
    private:
        int nid;
        int qsz;
        
        // int qp_pool_sz;
        // std::vector<struct ConnContext> quroum_conns;

        ScotConnContextPool* ctx_pool_rpli;
        ScotConnContextPool* ctx_pool_chkr;
        ScotConnContextPool* ctx_pool_drmr;

        MessageOut out;

        // Inner
        void __init_hartebeest();
        void __finalize_hartebeest();

    public:
        ScotConnection();
        ~ScotConnection();

        int get_my_nid();
        int get_quorum_sz();
        
        static ScotConnection& get_instance() {
            static ScotConnection quorum;
            return quorum;
        }

        void start();
        void end();

        ScotConnContextPool* get_rpli_ctx_pool();
        ScotConnContextPool* get_chkr_ctx_pool();
        ScotConnContextPool* get_drmr_ctx_pool();
    };
}