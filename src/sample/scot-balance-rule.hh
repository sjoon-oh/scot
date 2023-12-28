#pragma once

#include <cstdint>

#include "../includes/scot-rule.hh"

// typedef uint32_t (*SCOT_RULEF_T)(uint32_t);
namespace scot {

    void rule_balance_init(uint32_t);

    uint32_t rule_balance_2(uint32_t);
    uint32_t rule_balance_3(uint32_t);
}