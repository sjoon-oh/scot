#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include "./scot-rule-c.h"

#ifdef __cplusplus
extern "C" {
#endif

void scot_initialize();
void scot_finalize();

uint32_t scot_get_qs();
uint32_t scot_get_nid();

void scot_add_rule(uint32_t, SCOT_RULEF_T);
void scot_update_active(uint32_t);

int scot_propose(uint8_t*, uint16_t, uint8_t*, uint16_t, uint32_t);

// Timestamp related
int scot_timestamp_init(char*);
uint64_t scot_timestamp_record_start(int);
void scot_timestamp_record_end(int, uint64_t);

#ifdef __cplusplus
}
#endif