#pragma once

#include "../includes/scot-rule-c.h"

// typedef uint32_t (*SCOT_RULEF_T)(uint32_t);
#ifdef __cplusplus
extern "C" {
#endif

void rule_balance_init_c(uint32_t);

uint32_t rule_balance_2_c(uint32_t);
uint32_t rule_balance_3_c(uint32_t);

#ifdef __cplusplus
}
#endif