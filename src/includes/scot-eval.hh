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

        void __dump_statistics(std::string&, long long int*, uint64_t);
    
    public:
        ScotTimestamp(std::string, uint64_t = SCOT_TIMESTAMP_RECORDS);
        ~ScotTimestamp();

        inline uint64_t record_start() {

            uint64_t index = next.fetch_add(1);

            clock_gettime(CLOCK_MONOTONIC, &start[index]);
            return index;
        }

        inline void record_end(uint64_t index) {

            if (index > (SCOT_TIMESTAMP_RECORDS - 1)) 
                return;

            clock_gettime(CLOCK_MONOTONIC, &end[index]);
        }

        void dump_to_fs();
    };
}