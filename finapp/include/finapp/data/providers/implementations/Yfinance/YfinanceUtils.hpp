// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cpython/initconfig.h>
#include <pybind11/embed.h>  // everything needed for embedding

namespace finapp {

class PythonRuntime {
 public:
    static PythonRuntime& pythonRuntime() {
        static PythonRuntime instance;
        static pybind11::scoped_interpreter interpreter = [] {
            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            PyConfig_SetString(&config, &config.executable, L"/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python");
            return pybind11::scoped_interpreter{&config};
        }();
        static bool pathInit = [] {
            pybind11::module_::import("sys").attr("path").attr("insert")(0, FINAPP_PYTHON_DIR);
            return true;
        }();
        (void)pathInit;
        return instance;
    }

 private:
    PythonRuntime() = default;
    ~PythonRuntime() = default;
    PythonRuntime(const PythonRuntime&) = delete;
    PythonRuntime(PythonRuntime&&) = delete;
    PythonRuntime& operator=(PythonRuntime&&) = delete;
    PythonRuntime& operator=(const PythonRuntime&) = delete;
};
}  // namespace finapp
