#pragma once

#include <string>

#include "../../extern/spdlog/spdlog.h"
#include "../../extern/spdlog/sinks/stdout_color_sinks.h"
#include "../../extern/spdlog/sinks/basic_file_sink.h"

namespace scot {

    class MessageOut {
        std::string name;
        spdlog::logger* logger;
    
    public:
        MessageOut(const char*);

        spdlog::logger* get_logger();
    };
}

#define __SCOT_INFO__(LGR_INSTANCE, ...)   (LGR_INSTANCE).get_logger()->info(__VA_ARGS__)
#define __SCOT_WARN__(LGR_INSTANCE, ...)   (LGR_INSTANCE).get_logger()->warn(__VA_ARGS__)