/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include "./includes/scot-core.hh"
#include "./includes/scot-core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

scot::ScotCore* __scot_instance = nullptr;

void scot_initialize() {
    
    // Initialize
    scot::ScotCore scot_instance;
    __scot_instance = &scot_instance;

    // scot::rule_balance_init(scot_instance.get_qsize());
    scot_instance.initialize();
}


void scot_finalize() {
    if (__scot_instance != nullptr)
        __scot_instance->finish();
}


void scot_add_rule(uint32_t key, SCOT_RULEF_T rulef) {
    if (__scot_instance != nullptr)
        __scot_instance->add_rule(key, rulef);
}


void scot_update_active(uint32_t key) {
    if (__scot_instance != nullptr)
        __scot_instance->update_active(key);
}


#ifdef __cplusplus
}
#endif