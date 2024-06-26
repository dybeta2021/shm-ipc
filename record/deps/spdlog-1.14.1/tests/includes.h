#pragma once

#if defined(__GNUC__) && __GNUC__ == 12
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"  // Workaround for GCC 12
#endif
#include <catch2/catch_all.hpp>
#if defined(__GNUC__) && __GNUC__ == 12
    #pragma GCC diagnostic pop
#endif

#include "utils.h"
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <stdlib.h>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "../include/spdlog/spdlog.h"
#include "../include/spdlog/async.h"
#include "../include/spdlog/details/fmt_helper.h"
#include "../include/spdlog/mdc.h"
#include "../include/spdlog/sinks/basic_file_sink.h"
#include "../include/spdlog/sinks/daily_file_sink.h"
#include "../include/spdlog/sinks/null_sink.h"
#include "../include/spdlog/sinks/ostream_sink.h"
#include "../include/spdlog/sinks/rotating_file_sink.h"
#include "../include/spdlog/sinks/stdout_color_sinks.h"
#include "../include/spdlog/sinks/msvc_sink.h"
#include "../include/spdlog/pattern_formatter.h"
