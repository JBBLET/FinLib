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

        // Release the GIL after initialization so any C++ thread can acquire it via
        // py::gil_scoped_acquire. Without this, only the initializing thread holds the
        // GIL — other threads (e.g. gRPC worker pool) crash when calling Python.
        //
        // We must restore the GIL before ~scoped_interpreter() fires (which calls
        // Py_FinalizeEx). Static locals are destroyed in reverse construction order, so
        // gilGuard (constructed 5th) is destroyed BEFORE interpreter (constructed 2nd),
        // giving us the correct sequencing.
        struct GilRestoreGuard {
            PyThreadState* tstate = nullptr;
            ~GilRestoreGuard() {
                if (tstate && Py_IsInitialized()) {
                    PyEval_RestoreThread(tstate);
                }
            }
        };
        static GilRestoreGuard gilGuard;
        static bool gilReleased = [] {
            gilGuard.tstate = PyEval_SaveThread();
            return true;
        }();
        (void)gilReleased;
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
