#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <vector>
#include <atomic>

#include "../../../src/includes/scot-def.hh"
#include "../../../src/includes/scot-core.hh"

namespace scot_menc {


    class ScotMenciusReplicator : public scot::ScotWriter {
    private:
        int nid;

        std::atomic_flag sugg_lock;
        scot::MessageOut msg_out;     // Logger

    public:
        ScotMenciusReplicator(SCOT_LOGALIGN_T*, int);
        virtual ~ScotMenciusReplicator() = default;

        bool write_request(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t, uint8_t);
        
        // External handle
        void allow_write();
        bool try_write();
    };


    class ScotMenciusReplayer : public scot::ScotReader {
    private:
        std::thread worker;
        uint32_t worker_signal; // Worker thread only reads.

        bool skip_active;
        uint32_t menc_skip_delay;

        uint32_t id;

        ScotMenciusReplicator* rpli;

        void __worker(scot::ScotLog*, uint32_t, bool, uint32_t, ScotMenciusReplicator*);

    public:
        ScotMenciusReplayer(SCOT_LOGALIGN_T*, uint32_t, bool, uint32_t, ScotMenciusReplicator*);
        ~ScotMenciusReplayer();

        // void spawn_worker(SCOT_REPLAYER_WORKER_T);
        void worker_spawn();
        void worker_signal_toggle(uint32_t);
    };


    class ScotMenciusCore {
    private:
        uint32_t nid;
        uint32_t qsize;

        scot::ScotConfLoader ldr;

        ScotMenciusReplicator* rpli = nullptr;

        std::vector<ScotMenciusReplayer*> vec_rply; // Multiple Readers

        scot::ScotHeartbeat hb;
        scot::MessageOut msg_out;                   // Logger

        // 
        uint64_t rc_inline_max;
        uint32_t menc_skip_delay;

    public:
        ScotMenciusCore();
        ~ScotMenciusCore();

        void initialize();
        void finish();
        
        int propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t);

        uint32_t get_nid();
        uint32_t get_qsize();
    };
}

