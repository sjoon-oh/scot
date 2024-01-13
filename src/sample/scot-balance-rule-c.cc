#include "../includes/scot-rule-c.h"
#include "./scot-balance-rule-c.h"

static uint32_t qsize_c;

void rule_balance_init_c(uint32_t qs) {
    qsize_c = qs;
}


uint32_t rule_balance_2_c(uint32_t hashv) {
    return (hashv % qsize_c);
}


uint32_t rule_balance_3_c(uint32_t hashv) {
    return (hashv % qsize_c);
}