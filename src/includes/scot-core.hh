#pragma once

#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

#include "./scot-def.hh"
#include "./scot-slot.hh"
#include "./scot-log.hh"
#include "./scot-conn.hh"

#include "./lfmap.hh"

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

#define __START_WRITE__     while (writer_lock.test_and_set(std::memory_order_acquire)) {
#define __END_WRITE__       writer_lock.clear(std::memory_order_release); }

    public:
        ScotWriter(struct ScotAlignedLog*);
        ~ScotWriter() = default;
    };


    class ScotReader {
    protected:
        ScotLog log;
        std::thread worker;

    public:
        ScotReader(struct ScotAlignedLog*);
        ~ScotReader() = default;
    };


    class ScotReplicator final : public ScotWriter {
    private:
        scot::hash::LockfreeMap hasht;

        // Interface (in)
        void __ht_try_insert(struct ScotSlotEntry*);
        struct ScotSlotEntry* __ht_get_latest_entry(struct ScotSlotEntry*);

    public:
        ScotReplicator(struct ScotAlignedLog*);
        virtual ~ScotReplicator() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
    };


    class ScotReplayer final : public ScotReader {
    private:
    public:
        ScotReplayer(struct ScotAlignedLog*);
        ~ScotReplayer();
    };


    class ScotCore final {
    private:
        
        ScotReplicator* rpli = nullptr;
        ScotReplayer** rply = nullptr;  // Multiple Readers

        


    public:
        ScotCore();
        ~ScotCore();

        void initialize();
        void finish();

        // Interface
        int propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);

    };
}