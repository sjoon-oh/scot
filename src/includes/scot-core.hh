#pragma once

/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <map>
#include <string>
#include <vector>
#include <array>

#include <atomic>
#include <thread>

#include <memory>

#include "./scot-def.hh"
#include "./scot-slot.hh"
#include "./scot-log.hh"
#include "./scot-conn.hh"

#include "./scot-mout.hh"

#include "./lfmap.hh"
#include "./scot-hb.hh"

#include "../includes/scot-eval.hh"
#include "../includes/scot-rule.hh"

// User action
typedef void (*SCOT_USRACTION)(void*, size_t);

namespace scot {

    class ScotConfLoader {
    private:
        // Local attributes
        std::map<std::string, uint64_t> confs;

        MessageOut msg_out;

    public:
        ScotConfLoader();
        ~ScotConfLoader() = default;

        static ScotConfLoader& get_instance() {
            static ScotConfLoader loader;
            return loader;
        }

        uint64_t get_confval(const char*);
    };


    class ScotReplicator {
    private:
        ScotSlot    slot;
        uint32_t    pf_size;

        scot::hash::LockfreeMap hasht;
        ScotConnContextPool* ctx_pool;

        MessageOut msg_out;     // Logger

        // Interface (in)
        void __ht_try_insert(struct ScotSlotEntry*);
        struct ScotSlotEntry* __ht_get_latest_entry(struct ScotSlotEntry*);
        
    public:
        ScotReplicator(ScotConnContextPool*);
        ScotReplicator(uint32_t);
        virtual ~ScotReplicator() = default;

        uint32_t hash(char*, int);

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
    };


    class ScotChecker {
    private:
        ScotConnContextPool* ctx_pool;
        int wait_list_sz;

        struct ScotCheckerStatus {
            std::atomic_bool    in_wait;
            uint32_t            hashv;

            ScotCheckerStatus() : in_wait(ATOMIC_FLAG_INIT), hashv(0) {}
        };

        MessageOut  msg_out;     // Logger
        std::array<struct ScotCheckerStatus, SCOT_MAX_QPOOLSZ> wait_list;

        
    public:
        ScotChecker(ScotConnContextPool*);
        virtual ~ScotChecker() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint32_t);
        void release_wait(uint32_t);
    };


    class ScotReplayer {
    private:
        std::thread distr;
        uint32_t distr_signal; // Worker thread only reads.

        SCOT_LOGALIGN_T* gbase;

        uint32_t rnid;

        ScotChecker* chkr;
        SCOT_USRACTION user_action;

        std::vector<SCOT_LOGALIGN_T*> base_list;

        void __worker(uint32_t, ScotChecker*, SCOT_USRACTION = nullptr);

    public:
        ScotReplayer(uint32_t, SCOT_LOGALIGN_T*, ScotConnContextPool*, ScotChecker*, SCOT_USRACTION = nullptr);
        // ScotReplayer(SCOT_LOGALIGN_T*, uint32_t, ScotChecker*, SCOT_USRACTION = nullptr);
        ~ScotReplayer();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void distr_spawn();
        void distr_signal_toggle(uint32_t);
    };


    class ScotReceiver {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        SCOT_LOGALIGN_T* gbase;

        uint32_t rnid;

        ScotReplicator* rpli;
        SCOT_USRACTION user_action;

        std::vector<SCOT_LOGALIGN_T*> base_list;

        void __worker(uint32_t, ScotReplicator*, SCOT_USRACTION = nullptr);

    public:
        ScotReceiver(uint32_t, SCOT_LOGALIGN_T*, ScotConnContextPool*, ScotReplicator*, SCOT_USRACTION = nullptr);
        // ScotReceiver(SCOT_LOGALIGN_T*, uint32_t, ScotReplicator*, SCOT_USRACTION = nullptr);
        ~ScotReceiver();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };


    class ScotCore {
    private:
        uint32_t nid;
        uint32_t qsize;
        uint32_t qpool_size;

        ScotConfLoader ldr;
        ScotJudge<uint32_t> uint_judge;  // For now, key is used as int.

        ScotReplicator* rpli = nullptr;
        ScotChecker* chkr = nullptr;

        std::vector<ScotReplayer*> vec_rply;    // Multiple Readers
        std::vector<ScotReceiver*> vec_rcvr;    // Multiple Readers

        ScotHeartbeat hb;
        MessageOut msg_out;                     // Logger

        std::map<std::string, uint64_t> ext_confvar;

    public:
        ScotCore(SCOT_USRACTION = nullptr);
        ~ScotCore();

        void initialize();
        void finish();

        // Interface
        void add_rule(uint32_t, SCOT_RULEF_T);
        void update_active(uint32_t);
        
        uint32_t hash(char*, int);
        int propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t);

        uint32_t get_nid();
        uint32_t get_qsize();
    };
}

