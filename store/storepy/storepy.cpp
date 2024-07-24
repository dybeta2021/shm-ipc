//
// Created by 稻草人 on 2022/8/7.
//
// https://blog.csdn.net/qq_35608277/article/details/80071408

#include "utils.h"
#include "process_mutex.h"
#include "spdlog/spdlog.h"
#include "store.h"
#include <memory>
#include <mutex>
#include <pybind11/pybind11.h>
#include <thread>

namespace py = pybind11;


class StorePy {
private:
    std::unique_ptr<CProcessMutex> ptr_process_mutex = nullptr;
    std::mutex ptr_thread_lock;
    std::unique_ptr<Store> store_ = nullptr;

public:
    StorePy(const std::string &path,
            const int32_t &count,
            const int32_t &value_size,
            const bool &write_mode,
            const bool &init_header,
            const bool &init_disk,
            const std::string &log_path,
            const std::string &log_level,
            const bool &thread_lock = false,
            const bool &process_lock = false,
            const std::string &process_mutex = "process_mutex") {
        Logger::init_logger(log_path, log_level, false, false, false);

        store_ = std::make_unique<Store>();
        if (store_->Init(path, count, value_size, write_mode, init_disk, init_disk) != 0) {
            SPDLOG_ERROR("Store Init Failed.");
            throw std::runtime_error("Store Init Failed.");
        }

        if (process_lock && thread_lock) {
            SPDLOG_DEBUG("error config, process_lock true, thread_lock true!");
            throw std::invalid_argument("Invalid configuration: both process_lock and thread_lock are true.");
        }

        if (process_lock) {
            ptr_process_mutex = std::make_unique<CProcessMutex>(process_mutex.c_str());
        }
    }

    bool Set(const std::string &key,
             const char *value,
             const int32_t &value_len,
             const bool process_lock,
             const bool thread_lock) {
        store_data store_key{static_cast<int32_t>(key.length()), const_cast<char *>(key.c_str())};
        store_data store_value{value_len, const_cast<char *>(value)};

        if (process_lock) {
            if (!ptr_process_mutex) {
                SPDLOG_ERROR("ptr_process_mutex null.");
                return true;
            }

            if (!ptr_process_mutex->Lock()) {
                SPDLOG_ERROR("ptr_process_mutex lock error.");
                return true;
            }

            bool ret = store_->Set(store_key, store_value) != 0;
            ptr_process_mutex->UnLock();
            return ret;
        } else if (thread_lock) {
            std::lock_guard<std::mutex> lock(ptr_thread_lock);
            return store_->Set(store_key, store_value) != 0;
        } else {
            return store_->Set(store_key, store_value) != 0;
        }
    }

    bool Del(const std::string &key,
             const bool process_lock,
             const bool thread_lock) {
        store_data store_key{static_cast<int32_t>(key.length()), const_cast<char *>(key.c_str())};

        if (process_lock) {
            if (!ptr_process_mutex) {
                SPDLOG_ERROR("ptr_process_mutex null.");
                return true;
            }

            if (!ptr_process_mutex->Lock()) {
                SPDLOG_ERROR("ptr_process_mutex lock error.");
                return true;
            }

            bool ret = store_->Del(store_key) != 0;
            ptr_process_mutex->UnLock();
            return ret;
        } else if (thread_lock) {
            std::lock_guard<std::mutex> lock(ptr_thread_lock);
            return store_->Del(store_key) != 0;
        } else {
            return store_->Del(store_key) != 0;
        }
    }

    py::tuple Get(const std::string &key) {
        store_data store_key{static_cast<int32_t>(key.length()), const_cast<char *>(key.c_str())};
        store_data store_value{};

        if (store_->Get(store_key, store_value)) {
            return py::make_tuple(true, "null");
        }

        auto str = py::bytes(store_value.data, store_value.length);
        return py::make_tuple(false, str);
    }

    py::list GetCurrentAllKeys() {
        auto all_keys = store_->GetCurrentAllKeys();
        py::list out;
        for (const auto &item: all_keys) {
            out.append(py::bytes(item));
        }
        return out;
    }

    void ShowHeader() {
        store_->ShowHeader();
    }

    void ShowCurrentKey() {
        store_->ShowCurrentKey();
    }

    void ShowAllKey() {
        store_->ShowAllKey();
    }
};



PYBIND11_MODULE(storepy, m) {
    py::class_<StorePy>(m, "StorePy").def(py::init<const std::string &, const int32_t &, const int32_t &, const bool &, const bool &, const bool &, const std::string &, const std::string &, const bool &, const bool &, const std::string &>()).def("Set", &StorePy::Set, "Set", py::arg("key"), py::arg("value"), py::arg("value_len"), py::arg("process_lock"), py::arg("thread_lock")).def("Get", &StorePy::Get, "Get", py::arg("key")).def("Del", &StorePy::Del, "Del", py::arg("key"), py::arg("process_lock"), py::arg("thread_lock")).def("GetCurrentAllKeys", &StorePy::GetCurrentAllKeys, "GetCurrentAllKeys").def("ShowHeader", &StorePy::ShowHeader, "ShowHeader").def("ShowCurrentKey", &StorePy::ShowCurrentKey, "ShowCurrentKey").def("ShowAllKey", &StorePy::ShowAllKey, "ShowAllKey");
}
