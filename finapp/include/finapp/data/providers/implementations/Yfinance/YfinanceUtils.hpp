// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cpython/initconfig.h>
#include <pybind11/embed.h>  // everything needed for embedding

#include <cstdlib>
#include <string>

namespace finapp {

// Holds a single embedded CPython interpreter for the whole process. The first call
// initializes it; later calls just return the ready instance. Provider code uses it via
//   PythonRuntime::pythonRuntime();   // ensure interpreter is up
//   py::gil_scoped_acquire gil;       // take the GIL on this thread
//   py::module_::import("YFinanceFetcher");  // import a helper script as a module
class PythonRuntime {
 public:
    static PythonRuntime& pythonRuntime() {
        static PythonRuntime instance;
        static pybind11::scoped_interpreter interpreter = [] {
            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            // Pick the interpreter from the environment so no machine-specific path is baked in.
            //   FINAPP_PYTHON env → $VIRTUAL_ENV/bin/python env → FINAPP_PYTHON_DEFAULT
            //   (set at configure time via -DFINAPP_PYTHON=...) → libpython default.
            // The chosen interpreter is where pip-installed deps (e.g. yfinance) are found.
            std::string exe;
            if (const char* override = std::getenv("FINAPP_PYTHON")) {
                exe = override;
            } else if (const char* venv = std::getenv("VIRTUAL_ENV")) {
                exe = std::string(venv) + "/bin/python";
            }
#ifdef FINAPP_PYTHON_DEFAULT
            if (exe.empty()) exe = FINAPP_PYTHON_DEFAULT;
#endif
            if (!exe.empty()) {
                wchar_t* wexe = Py_DecodeLocale(exe.c_str(), nullptr);
                PyConfig_SetString(&config, &config.executable, wexe);
                PyMem_RawFree(wexe);
            }
            return pybind11::scoped_interpreter{&config};
        }();

        // One-time setup, runs while this thread still holds the GIL from interpreter init:
        //  - put the helper-script directory on sys.path so import("<stem>") resolves the .py
        //  - print a banner so it's obvious the embedded interpreter actually came up
        static bool runtimeInit = [] {
            namespace py = pybind11;
#ifdef FINAPP_PYTHON_DIR
            py::module_::import("sys").attr("path").attr("insert")(0, FINAPP_PYTHON_DIR);
#endif
            py::print("Hello World — finapp Python runtime ready");
            return true;
        }();
        (void)runtimeInit;

        // Release the GIL after initialization so any C++ thread can acquire it via
        // py::gil_scoped_acquire. Without this, only the initializing thread holds the
        // GIL — other threads (e.g. gRPC worker pool) crash when calling Python.
        //
        // We must restore the GIL before ~scoped_interpreter() fires (which calls
        // Py_FinalizeEx). Static locals are destroyed in reverse construction order, so
        // gilGuard (constructed after interpreter) is destroyed BEFORE interpreter,
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
