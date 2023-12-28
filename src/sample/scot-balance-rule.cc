#include "../includes/scot-rule.hh"
#include "./scot-balance-rule.hh"

static uint32_t qsize;


void scot::rule_balance_init(uint32_t qs) {
    qsize = qs;
}


uint32_t scot::rule_balance_2(uint32_t hashv) {
    return (hashv % qsize);
}


uint32_t scot::rule_balance_3(uint32_t hashv) {
    return (hashv % qsize);
}
