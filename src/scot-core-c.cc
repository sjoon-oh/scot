/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <vector>
#include <cstdio>

#include "./includes/scot-core.hh"
#include "./includes/scot-core-c.h"

scot::ScotCore* __scot_instance = nullptr;
std::vector<scot::ScotTimestamp*> __ts_recorder;

void scot_initialize() {
    
    // Initialize
    __scot_instance = new scot::ScotCore();
    __scot_instance->initialize();
}


void scot_finalize() {

    for (auto& ts: __ts_recorder) {
        ts->dump_to_fs();
    }

    if (__scot_instance != nullptr) {
        __scot_instance->finish();
        // delete __scot_instance;
    }
}


uint32_t scot_get_qs() {
    return __scot_instance->get_qsize();
}


uint32_t scot_get_nid() {
    return __scot_instance->get_nid();
}


void scot_add_rule(uint32_t key, SCOT_RULEF_T rulef) {
    if (__scot_instance != nullptr)
        __scot_instance->add_rule(key, rulef);
}


void scot_update_active(uint32_t key) {
    if (__scot_instance != nullptr)
        __scot_instance->update_active(key);
}


uint32_t scot_hash(char* buf, int buf_len) {
    return __scot_instance->hash(buf, buf_len);
}


int scot_propose(
    uint8_t* buf, uint16_t buf_sz, uint8_t* key, uint16_t key_sz, 
    uint32_t hashv) {

    if (__scot_instance != nullptr)
        __scot_instance->propose(buf, buf_sz, key, key_sz, hashv);
}


int scot_timestamp_init(char* name) {
    __ts_recorder.push_back(new scot::ScotTimestamp(std::string(name) + ".csv"));

    return (__ts_recorder.size() - 1);
}


uint64_t scot_timestamp_record_start(int handle) {
    return __ts_recorder[handle]->record_start();
}


void scot_timestamp_record_end(int handle, uint64_t index) {
    __ts_recorder[handle]->record_end(index);
}