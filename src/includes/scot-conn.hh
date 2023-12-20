#pragma once

#include <map>
#include <string>
#include <vector>

#include <infiniband/verbs.h>

namespace scot {
    struct ConnContext {
        int nid;
        struct  {
            struct ibv_qp* rpli_qp;     // Local connected QP
            struct ibv_qp* chkr_qp;     // Local connected QP
            struct ibv_mr* rpli_mr;
            struct ibv_mr* chkr_mr;
        } local;
        struct {
            struct ibv_mr* rply_mr;     // Local connected QP
            struct ibv_mr* rcvr_mr;     // Local connected QP
        } remote;
        ConnContext(int nid) : nid(nid) { 
            local = {nullptr, nullptr, nullptr, nullptr};
            remote = {nullptr, nullptr};
        }
    };

    // Keygen Helper

#define KEY_MR      0
#define KEY_QP      1
#define KEY_SCQ     2
#define KEY_RCQ     3

    std::string wkey_helper(const char*, int = -1, int = -1);
    std::string rkey_helper(const char*, int = -1, int = -1);

    class ScotConnection {
    private:
        std::string nid;
        
        int quorum_sz;
        std::vector<struct ConnContext> quroum_conns;

        // Inner
        void __init_hartebeest();
        void __finalize_hartebeest();

        void __connect_qps();
        void __wait_connection();

    public:
        ScotConnection();
        ~ScotConnection() = default;

        int get_my_nid();
        int get_quorum_sz();

        std::vector<struct ConnContext>& get_quorum();
        
        static ScotConnection& get_instance() {
            static ScotConnection quorum;
            return quorum;
        }

        void start();
        void end();
    };
}