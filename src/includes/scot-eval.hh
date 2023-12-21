#pragma once

#include <unistd.h>
#include <ctime>
#include <cstdint>

#include <atomic>
#include <string>

#include "./scot-def.hh"

#define __ELAPSED_NSEC__(START, END) \
    (END.tv_nsec + END.tv_sec * 1000000000UL - \
        START.tv_nsec - START.tv_sec * 1000000000UL)


namespace scot {

    class ScotTimestamp final {
    private:
        const uint64_t rec_sz;

        struct timespec* start;
        struct timespec* end;

        std::atomic_uint64_t next;

        // out
        std::string fname;
    
    public:
        ScotTimestamp(std::string, uint64_t = SCOT_TIMESTAMP_RECORDS);
        ~ScotTimestamp();

        uint64_t record_start();
        void record_end(uint64_t);

        void dump_to_fs();
    };
}