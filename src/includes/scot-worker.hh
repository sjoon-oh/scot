

#include <cstdint>

namespace scot {

    void worker_replayer(uint8_t*);
}

typedef     void (*SCOT_REPLAYER_WORKER_T)(uint8_t*);
