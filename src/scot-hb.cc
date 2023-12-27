#include "../extern/nlohmann/json.hpp"
#include "../hartebeest/src/includes/hartebeest-c.h"

#include "./includes/scot-def.hh"
#include "./includes/scot-core.hh"


void scot::ScotHeartbeat::__beater(SCOT_SCB_FINEGRAINED_T* scoreboard, struct ScotScoreboardTable* table) {


}


scot::ScotHeartbeat::ScotHeartbeat(SCOT_SCB_FINEGRAINED_T* addr) 
    : scb_open(addr), beater() {

}


scot::ScotHeartbeat::~ScotHeartbeat() {
    
}


void scot::ScotHeartbeat::beater_spawn() {
    beater = std::thread(
        [&]() {
            __beater(this->scb_open, &(this->scb_table)); 
        }
    );
    beater.detach();
}


void scot::ScotHeartbeat::register_scb(SCOT_SCB_FINEGRAINED_T* addr) {
    if (scb_open == nullptr)
        scb_open = addr;
}
