#pragma once

#include <memory>
#include <functional>

#include <map>

#include <cstdint>

#include "./scot-def.hh"

typedef uint32_t (*SCOT_RULEF_T)(uint32_t);

namespace scot {

    //
    // Rule should always return who owns what.
    struct ScotOwnership {
        uint32_t owner;
        SCOT_RULEF_T rule;
    };


    // Rule
    template <class T>
    class ScotJudge {
    private:
        std::map<T, SCOT_RULEF_T> rdict;
        SCOT_RULEF_T active_rule = nullptr;

        std::atomic_flag on_update;

    public:
        ScotJudge() { on_update.clear(); };
        ~ScotJudge() = default;

        void update_active(T key) { active_rule = rdict.at(key); }
        void register_rule(T key, SCOT_RULEF_T rule) { 
            rdict.insert(std::make_pair(key, rule));
        }

        inline void judge(struct ScotOwnership* res, uint32_t hashv) {

            res->owner = active_rule(hashv);
            res->rule = active_rule;
        }
    };
}