/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <regex>
#include <string>
#include <iostream>
#include <vector>
#include <atomic>

#include <utility>

#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


scot::ScotChecker::ScotChecker(scot::ScotConnContextPool* pool) 
    : ctx_pool(pool), msg_out("chkr") { 
    
    assert(ctx_pool != nullptr);

    for (int idx = 0; idx < ctx_pool->get_qp_pool_sz(); idx++) {
        // Do something here?
    }
}

bool scot::ScotChecker::write_request(
    uint8_t* buf, uint16_t buf_sz, 
    uint8_t* key, uint16_t key_sz, 
    uint32_t hashv, uint32_t owner) {

    struct ScotSlotEntry local_entry;
    local_entry.hashv      = hashv;
    local_entry.buffer     = buf;
    local_entry.buffer_sz  = buf_sz;
    local_entry.key        = key;
    local_entry.key_sz     = key_sz;
    local_entry.msg        = SCOT_MSGTYPE_WAIT;

    SCOT_QPOOL_START

    // This is blocking function.
    std::vector<struct scot::ConnContext>* qlist = ctx_pool->pop_qlist();

    // Per-qpool
    struct ScotAlignedLog* log = reinterpret_cast<struct ScotAlignedLog*>(
            uintptr_t((*qlist)[0].local.mr->addr) + uintptr_t((*qlist)[0].offset)
        );
    int qpool_sid = (*qlist)[0].qpool_sid;
    
    // Register the status.
    wait_list[qpool_sid].hashv = hashv;

    SCOT_LOGALIGN_T* header = write_local_log(log, &local_entry, 0);
    size_t logsz = sizeof(struct ScotMessageHeader) + 
                static_cast<size_t>(local_entry.buffer_sz) + sizeof(uint8_t);

    size_t offset = uintptr_t(header) - uintptr_t(log);
    SCOT_LOGALIGN_T* remote_target_addr;

    bool ret = 0;
    for (auto& ctx: (*qlist)) {

        remote_target_addr = 
            reinterpret_cast<SCOT_LOGALIGN_T*>(
                    uintptr_t(ctx.remote.mr->addr) + uintptr_t(ctx.offset) + offset);

        if (owner == ctx.rnid) {

            reinterpret_cast<struct ScotMessageHeader*>(header)->msg = 
                SCOT_MSGTYPE_WAIT;

            ret = hartebeest_rdma_post_single_signaled_inline(
                ctx.local.qp,           // Local QP
                header,                 // Local starting point of a RDMA message
                remote_target_addr,     // To where at remote?
                logsz,                  // Total message size
                IBV_WR_RDMA_WRITE,
                ctx.local.mr->lkey,     // Local MR acces key
                ctx.remote.mr->rkey,    // Remote MR access key
                ctx.rnid);

#ifdef __DEBUG__
            if (ret == false) {
                __SCOT_WARN__(msg_out, "→→ RDMA Write failed");
                // assert(0);
            }
#endif


        } else {

            reinterpret_cast<struct ScotMessageHeader*>(header)->msg = 
                SCOT_MSGTYPE_HDRONLY;

            ret = hartebeest_rdma_post_single_signaled_inline(
                ctx.local.qp,           // Local QP
                header,                 // Local starting point of a RDMA message
                remote_target_addr,     // To where at remote?
                sizeof(struct ScotMessageHeader),                 
                                        // Total message size
                IBV_WR_RDMA_WRITE,
                ctx.local.mr->lkey,     // Local MR acces key
                ctx.remote.mr->rkey,    // Remote MR access key
                ctx.rnid);

#ifdef __DEBUG__
            if (ret == false) {
                __SCOT_WARN__(msg_out, "→→ RDMA Write failed");
                // assert(0);
            }
#endif
    
        }

        ret = hartebeest_rdma_send_poll(ctx.local.qp);

#ifdef __DEBUG__
            if (ret == false)
                __SCOT_WARN__(msg_out, "→→ CQ poll not successful");
#endif
    }

    wait_list[qpool_sid].in_wait.store(1, std::memory_order_release);

    while (wait_list[qpool_sid].in_wait.load(std::memory_order_acquire) == 0)
        ;

    ctx_pool->push_qlist(qlist);
    SCOT_QPOOL_END

#ifdef __DEBUG__
    __SCOT_INFO__(msg_out, "→→ {} END", __func__);
#endif
    
    return 0;
}


void scot::ScotChecker::release_wait(uint32_t hashv) {

#ifdef __DEBUG__
        __SCOT_INFO__(msg_out, "→→ Trying release for {}", hashv);
#endif

    for (int idx = 0; idx < wait_list_sz; idx) {
        
        if (wait_list[idx].in_wait.load(std::memory_order_acquire) == 1) {
            if (wait_list[idx].hashv == hashv) {
                wait_list[idx].in_wait.store(0, std::memory_order_release);
#ifdef __DEBUG__
                __SCOT_INFO__(msg_out, "→→ Checker released, for {}", hashv);
#endif
            }
        }
    }
}