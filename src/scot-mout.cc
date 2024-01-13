/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <string>

#include <assert.h>
#include <stdlib.h>

#include "../extern/spdlog/spdlog.h"
#include "./includes/scot-mout.hh"

scot::MessageOut::MessageOut(const char* lg_name) {
    name = std::string(lg_name);
    logger = spdlog::stdout_color_mt(name).get();
    spdlog::set_pattern("[%n:%^%l%$] %v");
}


spdlog::logger* scot::MessageOut::get_logger() {
    return logger;
};