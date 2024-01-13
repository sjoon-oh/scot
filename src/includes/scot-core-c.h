#pragma once
/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include "./scot-rule.hh"

#ifdef __cplusplus
extern "C" {
#endif

void scot_initialize();
void scot_finalize();

void scot_add_rule(uint32_t, SCOT_RULEF_T);
void scot_update_active(uint32_t);

#ifdef __cplusplus
}
#endif