#pragma once

/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <map>
#include <string>
#include <vector>
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
    

    // Writer interface
    class ScotWriter {
    protected:
        ScotSlot    slot;
        ScotLog     log;

        std::atomic_flag writer_lock;

#define __START_WRITE__     while (writer_lock.test_and_set(std::memory_order_acquire)) { }
#define __END_WRITE__       writer_lock.clear(std::memory_order_release); { }

    public:
        ScotWriter(SCOT_LOGALIGN_T*);
        ~ScotWriter() = default;

        SCOT_LOGALIGN_T* get_base();
    };


    class ScotReader {
    protected:
        ScotLog log;

    public:
        ScotReader(SCOT_LOGALIGN_T*);
        ~ScotReader() = default;
    };


    class ScotReplicator : public ScotWriter {
    private:
        scot::hash::LockfreeMap hasht;

        MessageOut msg_out;     // Logger

        // Interface (in)
        void __ht_try_insert(struct ScotSlotEntry*);
        struct ScotSlotEntry* __ht_get_latest_entry(struct ScotSlotEntry*);

    public:
        ScotReplicator(SCOT_LOGALIGN_T*);
        virtual ~ScotReplicator() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
    };


    class ScotChecker : public ScotWriter {
    private:
        MessageOut msg_out;     // Logger

        uint32_t wait_hashv;
        std::atomic_flag wait_lock;

#define __WAIT_FOR_PROPACK__    do { wait_lock.test_and_set(std::memory_order_acquire); } while(0);
#define __RELEASE_AT_PROPACK__  wait_lock.clear(std::memory_order_release);

    public:
        ScotChecker(SCOT_LOGALIGN_T*);
        virtual ~ScotChecker() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint32_t);
        void release_wait(uint32_t);
    };


    class ScotReplayer : public ScotReader {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        uint32_t id;
        ScotChecker* chkr;

        void __worker(struct ScotLog*, uint32_t, ScotChecker*);

    public:
        ScotReplayer(SCOT_LOGALIGN_T*, uint32_t, ScotChecker*);
        ~ScotReplayer();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };


    class ScotReceiver : public ScotReader {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        uint32_t id;

        ScotReplicator* rpli;

        void __worker(struct ScotLog*, uint32_t, ScotReplicator*);

    public:
        ScotReceiver(SCOT_LOGALIGN_T*, uint32_t, ScotReplicator*);
        ~ScotReceiver();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };

    class ScotCore {
    private:
        uint32_t nid;
        uint32_t qsize;

        ScotConfLoader ldr;
        ScotJudge<uint32_t> uint_judge;  // For now, key is used as int.

        ScotReplicator* rpli = nullptr;
        ScotChecker* chkr = nullptr;

        std::vector<ScotReplayer*> vec_rply;    // Multiple Readers
        std::vector<ScotReceiver*> vec_rcvr;    // Multiple Readers

        ScotHeartbeat hb;
        MessageOut msg_out;                     // Logger

        // 
        uint64_t rc_inline_max;

    public:
        ScotCore();
        ~ScotCore();

        void initialize();
        void finish();

        // Interface
        void add_rule(uint32_t, SCOT_RULEF_T);
        void update_active(uint32_t);
        
        int propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t);

        uint32_t get_nid();
        uint32_t get_qsize();
    };
}