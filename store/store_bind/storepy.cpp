//
// Created by 稻草人 on 2022/8/7.
//
// https://blog.csdn.net/qq_35608277/article/details/80071408
#include "interface.h"
#include "logger.h"


PYBIND11_MODULE(storepy, m) {
    py::class_<StorePy>(m, "StorePy")
            .def(py::init<const std::string &,
                          const size_t &,
                          const size_t &,
                          const bool &,
                          const bool &,
                          const bool &,
                          const std::string &,
                          const std::string &,
                          const bool &,
                          const bool &,
                          const std::string &>())

            .def("Set", &StorePy::Set, "Set", py::arg("key"), py::arg("value"), py::arg("value_len"), py::arg("process_lock"), py::arg("thread_lock"))
            .def("Get", &StorePy::Get, "Get", py::arg("key"))
            .def("Del", &StorePy::Del, "Del", py::arg("key"), py::arg("process_lock"), py::arg("thread_lock"))
            .def("GetCurrentAllKeys", &StorePy::GetCurrentAllKeys, "GetCurrentAllKeys")
            .def("ShowHeader", &StorePy::ShowHeader, "ShowHeader")
            .def("ShowCurrentKey", &StorePy::ShowCurrentKey, "ShowCurrentKey")
            .def("ShowAllKey", &StorePy::ShowAllKey, "ShowAllKey");
}
