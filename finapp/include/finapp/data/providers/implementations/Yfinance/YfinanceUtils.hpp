// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cpython/initconfig.h>
#include <pybind11/embed.h>  // everything needed for embedding
#include <pybind11/eval.h>   // pybind11::exec

#include <cstdlib>
#include <string>

#include "finapp/EmbeddedPythonScripts.hpp"

namespace finapp {

class PythonRuntime {
 public:
    static PythonRuntime& pythonRuntime() {
        static PythonRuntime instance;
        static pybind11::scoped_interpreter interpreter = [] {
            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            // Resolve the interpreter without baking in a developer-machine path. Precedence:
            //   FINAPP_PYTHON env → $VIRTUAL_ENV/bin/python env → FINAPP_PYTHON_DEFAULT (set at
            //   configure time via -DFINAPP_PYTHON=...) → libpython default.
            // The chosen interpreter determines where pip-installed deps (e.g. yfinance) are found.
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
        // Register every embedded script as an in-memory module named by its filename stem, so
        // pybind11::module_::import("<stem>") works with nothing on disk. Script-agnostic: new
        // scripts in FINAPP_EMBEDDED_SCRIPTS are picked up automatically.
        static bool moduleInit = [] {
            namespace py = pybind11;
            py::object moduleType = py::module_::import("types").attr("ModuleType");
            py::dict sysModules = py::module_::import("sys").attr("modules");
            for (const auto& script : kEmbeddedPythonScripts) {
                std::string name(script.name);
                py::object mod = moduleType(name);
                mod.attr("__file__") = "<embedded:" + name + ">";
                py::exec(std::string(script.source), mod.attr("__dict__"));
                sysModules[name.c_str()] = mod;
            }
            return true;
        }();
        (void)moduleInit;

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
