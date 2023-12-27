#pragma once

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

#include "./scot-worker.hh"
#include "./scot-mout.hh"

#include "./lfmap.hh"
#include "./scot-hb.hh"

#include "../includes/scot-eval.hh"

namespace scot {
    class ScotConfLoader {
    private:

    public:
        ScotConfLoader();
        ~ScotConfLoader() = default;

        static ScotConfLoader& get_instance() {
            static ScotConfLoader loader;
            return loader;
        }
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


    class ScotReplicator final : public ScotWriter {
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


    class ScotChecker final : public ScotWriter {
    private:
        MessageOut msg_out;     // Logger

    public:
        ScotChecker(SCOT_LOGALIGN_T*);
        virtual ~ScotChecker() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
    };


    class ScotReplayer final : public ScotReader {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        uint32_t id;

        void __worker(struct ScotLog*, uint32_t);

    public:
        ScotReplayer(SCOT_LOGALIGN_T*, uint32_t);
        ~ScotReplayer();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };


    class ScotReceiver final : public ScotReader {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        uint32_t id;

        void __worker(struct ScotLog*, uint32_t);

    public:
        ScotReceiver(SCOT_LOGALIGN_T*, uint32_t);
        ~ScotReceiver();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };


    class ScotCore final {
    private:
        ScotReplicator* rpli = nullptr;
        ScotChecker* chkr = nullptr;

        std::vector<ScotReplayer*> vec_rply;    // Multiple Readers
        std::vector<ScotReceiver*> vec_rcvr;    // Multiple Readers

        ScotHeartbeat hb;

        MessageOut msg_out;                     // Logger

    public:
        ScotCore();
        ~ScotCore();

        void initialize();
        void finish();

        // Interface
        int propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);

    };
}