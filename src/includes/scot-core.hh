#pragma once

#include <map>
#include <string>
#include <vector>
#include <atomic>

#include "./scot-def.hh"
#include "./scot-log.hh"
#include "./scot-slot.hh"
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

        virtual bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t) = 0;
    };


    class ScotReader {
    protected:
        ScotLog log;

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

        

        virtual bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
    };


    class ScotReplayer : ScotReader {

    };


    class ScotCore {
    private:
        ScotReplicator* rpli = nullptr;
        ScotReplayer* rply = nullptr;

    public:
        ScotCore();
        ~ScotCore() = default;

        void initialize();
        void finish();

        static ScotCore& get_instance() {
            static ScotCore core;
            return core;
        }
    };
}