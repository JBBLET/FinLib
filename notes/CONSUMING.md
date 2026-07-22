# Consuming FinLib / finapp as a dependency

This guide is for a **client developer** who wants to reuse finapp's components — for
example the bundled `YFinanceProvider` / `YFinanceEquityProvider` — without copying source.

## TL;DR

```cmake
include(FetchContent)

# Build finapp against the SAME Python you will use at runtime.
# The interpreter's X.Y version MUST match the venv you point FINAPP_PYTHON at (see below).
set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python")

# Bake that interpreter as finapp's runtime default. Env vars FINAPP_PYTHON / VIRTUAL_ENV
# still override it at runtime, so this is just a convenient default.
set(FINAPP_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python")

FetchContent_Declare(
    finlibRepo
    GIT_REPOSITORY "https://…/FinLib.git"   # or file:///abs/path/to/FinLib/.git for a local clone
    GIT_TAG        main                      # a branch/tag that contains the commits you want
)
FetchContent_MakeAvailable(finlibRepo)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE finapp_providers)
```

That's it — you do **not** need to disable tests; FinLib only builds its tests/benchmarks
when it is the top-level project (see "Dev vs. consumption" below).

## Library vs. application build

The root `CMakeLists.txt` is **consumption-aware**:

- **As a dependency** (`add_subdirectory` / `FetchContent`) it builds only the library
  targets and their real dependencies (Eigen, Python3 + pybind11). It does **not** fetch
  googletest, build tests, or assume any `.venv` layout.
- **As the top-level project** (standalone dev) it additionally pulls in the in-repo
  `.venv`, tests, and benchmarks. All of that lives in `cmake/DevBuild.cmake` and runs only
  when `PROJECT_IS_TOP_LEVEL`.

Available library targets include `finapp_providers`, `finapp_service`, `finapp_core`,
`finapp_csv_repository`, and the `finlib_*` libraries.

## Python: how the interpreter is chosen

`finapp_providers` embeds CPython (pybind11) and bundles its Python helper scripts
**inside the binary** (no script files need to exist on disk at runtime). At first use it
resolves the interpreter in this order:

1. `FINAPP_PYTHON` environment variable
2. `VIRTUAL_ENV` environment variable → `$VIRTUAL_ENV/bin/python`
3. `FINAPP_PYTHON_DEFAULT` — baked at build time from the CMake `-DFINAPP_PYTHON=…` value
4. the interpreter finapp was linked against (libpython default)

The chosen interpreter is where pip-installed packages (e.g. `yfinance`) are found.

### ⚠️ The version-match rule (most common mistake)

`pybind11::embed` links a specific **libpython X.Y**, decided by `Python3_EXECUTABLE` at
build time. At runtime you can only point the interpreter at a venv of the **same X.Y**.
Pointing it at a different version's venv fails with `ModuleNotFoundError` because the
linked libpython looks in `python<X.Y>/site-packages`, which the other venv doesn't have.

➡️ **Build against the same venv you'll run against**: set `Python3_EXECUTABLE` and
`FINAPP_PYTHON` to the same interpreter.

## Build-time dependency check

At configure time finapp verifies the target interpreter has the packages declared in the
project's `pyproject.toml` (`[project].dependencies`), checked by distribution name:

- All present → `-- finapp: pyproject.toml Python deps satisfied in '…'`
- Missing → a **warning** listing the packages and a ready-to-run `pip install …` command.
- Set `-DFINAPP_REQUIRE_PYTHON_DEPS=ON` to turn that warning into a **hard configure error**.

Pinned versions in `pyproject.toml` are treated as informational; only *presence* is
required, so your venv's transitive versions may differ.

## Adding more bundled scripts (maintainers)

Embedded scripts are listed in `finapp/CMakeLists.txt` under `FINAPP_EMBEDDED_SCRIPTS`.
Each `.py` is embedded at build time and importable in C++ by its filename stem
(`YFinanceFetcher.py` → `py::module_::import("YFinanceFetcher")`). Add a line to that list
— no C++ changes needed.
