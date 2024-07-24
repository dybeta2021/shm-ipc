//
// Created by Gavin on 2024/7/16.
//

#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE// 必须定义这个宏,才能输出文件名和行号
#define SPDLOG_LOG_PATTERN "[%m/%d %T.%F] [%^%=8l%$] [%6P/%-6t] [%@#%!] %v"

#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>


class Utils {
public:
    static bool remove_file(const std::string &path) {
        if (remove(path.c_str()) == -1) {
            SPDLOG_WARN("Failed to remove: {}, errno: {}", path, strerror(errno));
            return false;
        }
        SPDLOG_DEBUG("Removed: {}.", path);
        return true;
    }

    static bool check_file_exists(const std::string &path) {
        bool exists = std::filesystem::exists(path);
        SPDLOG_INFO("File {} exists: {}.", path, exists ? "yes" : "no");
        return exists;
    }

    static void timing_ms_sleep(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    static void timing_ms_busy_wait(int ms) {
        auto us = ms * 1000;
        auto start = std::chrono::system_clock::now();
        while (true) {
            auto end = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            if (duration.count() > us) break;
        }
    }

    static void create_folder(const std::string &dir_path) {
        std::filesystem::path dir(dir_path);

        if (!std::filesystem::exists(dir)) {
            try {
                if (std::filesystem::create_directory(dir)) {
                    std::cout << "Create directory successfully." << std::endl;
                } else {
                    std::cerr << "Failed to create directory." << std::endl;
                }
            } catch (const std::filesystem::filesystem_error &e) {
                std::cerr << "Error: " << e.what() << std::endl;
                throw;
            }
        } else {
            std::cout << "This directory already exists." << std::endl;
        }
    }
};

class Logger {
public:
    static bool init_console_logger(const std::string &level = "trace", bool show_use_level = false, bool use_format = false) {
        try {
            if (use_format) {
                spdlog::set_pattern(SPDLOG_LOG_PATTERN);
            }

            set_log_level(level);

            if (show_use_level) {
                log_messages();
            }
            return true;
        } catch (const spdlog::spdlog_ex &ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            return false;
        }
    }

    static bool init_logger(const std::string &log_file = "clogs/daily.log",
                            const std::string &level = "trace",
                            bool use_async = false,
                            bool show_use_level = true,
                            bool use_format = false) {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_file, 4, 0);

            if (use_async) {
                spdlog::init_thread_pool(8192, 1);
                std::vector<spdlog::sink_ptr> log_sinks{console_sink, daily_sink};
                auto logger = std::make_shared<spdlog::async_logger>("logger", log_sinks.begin(), log_sinks.end(),
                                                                     spdlog::thread_pool(),
                                                                     spdlog::async_overflow_policy::block);
                if (use_format) {
                    logger->set_pattern(SPDLOG_LOG_PATTERN);
                }
                spdlog::set_default_logger(logger);
            } else {
                spdlog::sinks_init_list log_sinks = {console_sink, daily_sink};
                auto logger = std::make_shared<spdlog::logger>("logger", log_sinks);
                if (use_format) {
                    logger->set_pattern(SPDLOG_LOG_PATTERN);
                }
                spdlog::set_default_logger(logger);
            }

            set_log_level(level);

            if (show_use_level) {
                log_messages();
            }

            return true;
        } catch (const spdlog::spdlog_ex &ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            return false;
        }
    }

private:
    static void set_log_level(const std::string &level) {
        if (level == "trace") {
            spdlog::set_level(spdlog::level::trace);
        } else if (level == "debug") {
            spdlog::set_level(spdlog::level::debug);
        } else if (level == "info") {
            spdlog::set_level(spdlog::level::info);
        } else if (level == "warn") {
            spdlog::set_level(spdlog::level::warn);
        } else if (level == "error") {
            spdlog::set_level(spdlog::level::err);
        } else if (level == "critical") {
            spdlog::set_level(spdlog::level::critical);
        } else if (level == "off") {
            spdlog::set_level(spdlog::level::off);
        } else {
            std::cerr << "Level should be in [trace, debug, info, warn, error, critical, off]!";
            spdlog::set_level(spdlog::level::trace);
        }
    }

    static void log_messages() {
        SPDLOG_TRACE("Trace from SpdLog!");
        SPDLOG_DEBUG("Debug from SpdLog!");
        SPDLOG_INFO("Info from SpdLog!");
        SPDLOG_WARN("Warn from SpdLog!");
        SPDLOG_ERROR("Error from SpdLog!");
        SPDLOG_CRITICAL("Critical from SpdLog!");
    }
};
