# Technical Specification: deepenc — AI-Driven VVenC Optimization Fork

HAS_SUB_MODULES=true

## Overview

deepenc is a fork of [VVenC](https://github.com/fraunhoferhhi/vvenc) (Fraunhofer Versatile Video Encoder) that integrates AI-driven kernel optimization capabilities. This document describes the modifications and instrumentation added to the VVenC C/C++ source code.

## Scope

This specification covers the deepenc source fork only. The harness tooling (trace generation, optimization agent, test pyramid, etc.) is specified in `deepenc-harness/technical-specification.md`.

## Planned Modifications

- Instrumentation hooks for hot function tracing
- Side-channel decision log emission for metadata collection
- CMake build system integration points
- Compatibility APIs for the harness tooling

_This document is a placeholder draft and will be refined as development progresses._

## C++ Coding Conventions

This section documents the C++ idioms, naming conventions, and patterns used throughout the deepenc codebase. All generated specifications and class declarations must follow these conventions.

### Language & Build

- **C++14** target (`-std=c++14`). No C++17 or later features.
- **Include guard**: `#pragma once` only (no `#ifndef`).
- **Namespace**: Everything lives in `namespace vvenc { ... }`.
- **Build system**: CMake (minimum 3.13) with a GNU Make convenience wrapper.
- **License header**: Clear BSD License block at the top of every file.

### Documentation

- **Doxygen**: `/** \file <name> \brief <summary> */` at the top of every file.
- **Method docs**: `\param[in]`, `\param[out]`, `\retval` tags.
- **Group tags**: `\ingroup VVEnc`, `\ingroup CommonLib`, `\ingroup EncoderLib`.

### Naming Conventions

| Category | Pattern | Example |
|---|---|---|
| Classes | PascalCase | `VVEncImpl`, `EncLib`, `NoMallocThreadPool` |
| Methods | PascalCase | `initEncoderLib()`, `encodePicture()` |
| Member variables | `m_` prefix | `m_pEncLib`, `m_eState`, `m_bInitialized`, `m_cErrorString`, `m_picSharedList` |
| Member pointer | `m_p` prefix | `m_pEncLib`, `m_preEncoder` |
| Member bool | `m_b` prefix | `m_bInitialized`, `m_accessUnitOutputStarted` |
| Member enum | `m_e` prefix | `m_eState` |
| Member string | `m_c` prefix | `m_cErrorString`, `m_cEncoderInfo` |
| Private helpers | `x` prefix | `xGetAccessUnitsSize()`, `xUninitLib()` |
| Constants | `static constexpr` | `MAX_CU_DEPTH`, `MAX_TB_SIZEY` |
| Macros | `UPPER_CASE` | `VVENC_MAX_GOP`, `ENABLE_SIMD_OPT` |
| C API types | `vvenc` prefix | `vvencEncoder`, `vvenc_config`, `vvencYUVBuffer` |
| C API functions | `vvenc_` prefix | `vvenc_encoder_create()`, `vvenc_encode()` |

### Class Structure

- **No inheritance** — plain classes with composition and forward declarations.
- **Virtual destructor** — always present (`virtual ~ClassName()`) even for non-polymorphic classes.
- **In-class member initialization** — prefer `Type m_member = value` over constructor init-lists.
- **Forward declarations** — used extensively for dependent classes (e.g., `class EncLib;`).
- **`explicit`** — used on single-argument constructors where applicable.

### Method Signatures

```cpp
// Return int for error codes (0 = success)
int init(vvenc_config* config);

// Output parameters via non-const pointers
int encode(vvencYUVBuffer* pcYUVBuffer, vvencAccessUnit* pcAccessUnit, bool* pbEncodeDone);

// Const-correctness on getters and read-only parameters
int getConfig(vvenc_config& rcVVEncCfg) const;
int checkConfig(const vvenc_config& rcVVEncCfg);

// Static factory/utility methods
static const char* getVersionNumber();
static std::string createEncoderInfoStr();

// Private helpers prefixed with x
int xGetAccessUnitsSize(const AccessUnitList& rcAuList);
```

### Constants & Enums

- **`static constexpr`** is preferred over `#define` for numeric constants.
- **Plain `enum`** inside class scope (not `enum class`):
  ```cpp
  class VVEncImpl {
  public:
    enum VVEncInternalState {
      INTERNAL_STATE_UNINITIALIZED = 0,
      INTERNAL_STATE_INITIALIZED   = 1,
    };
  };
  ```
- **`typedef enum`** in the C public API layer (C-compatible):
  ```c
  typedef enum { VVENC_OK = 0, VVENC_ERR_UNSPECIFIED = 1 } vvencErrCodes;
  ```
- **`union` with bitfields** for compact packed representations:
  ```cpp
  union MmvdIdx {
    using T = uint8_t;
    struct {
      T position : 2;
      T step     : 3;
    } pos;
    T val;
  };
  ```

### Memory Management

- **C++ code**: `new` / `delete`, `new[]` / `delete[]`.
- **C API wrappers**: `malloc()` / `free()`.
- **`nullptr`** used consistently (never `NULL`).
- **Guard deallocation** with `if (ptr) { delete ptr; ptr = nullptr; }`.

### Error Signaling

- `int` return code: `0` = success, negative/positive = error.
- Error messages retrieved via `getLastError()` / `getErrorMsg(int ret)`.
- Internal pattern: `setAndRetErrorMsg(int Ret)` stores the error string for retrieval.
- C API: `vvenc_get_last_error(enc)` returns `const char*`.

### STL & Threading

| Library | Usage |
|---|---|
| `std::vector` | Dynamic arrays, pipeline stage lists |
| `std::list` | Picture shared list (`PicShared*`) |
| `std::deque` | Access unit output queue |
| `std::string` | Error messages, encoder info |
| `std::function` | Callback registration (`setRecYUVBufferCallback`) |
| `std::mutex` + `std::condition_variable` | Pipeline stage synchronization |
| `std::pair` / `std::tuple` | Lightweight compound returns |

### Callback Pattern

```cpp
// Registration
std::function<void(void*, vvencYUVBuffer*)> m_recYuvBufFunc;
void*                                         m_recYuvBufCtx;

// C API callback typedef
typedef void (*vvencLoggingCallback)(void*, int, const char*, va_list);
```

### Test Conventions

- **C API tests** (`test/vvencinterfacetest/`): Plain `.c` files with `printf`/`return` pattern.
- **C++ SDK tests** (`test/vvenclibtest/`): Custom macros:
  ```cpp
  #define TEST(x)   { int res = x; g_numTests++; g_numFails += res; }
  #define TESTT(x,w){ int res = x; g_numTests++; g_numFails += res; }
  #define ERROR(w)  { g_numTests++; g_numFails++; }
  ```
- **Global counters**: `int g_numTests`, `int g_numFails`.
- **Manual runner**: `int main(int argc, char* argv[])` with `switch(testId)`.
- **Unit tests** (`test/vvenc_unit_test/`): Template-based comparison helpers:
  ```cpp
  template<typename T>
  static inline bool compare_value(const std::string& context, const T ref, const T opt);
  ```
- **Strict comparison**: `ref == opt` checks, verbose `std::cerr` on failure, no test framework dependency.

### Preprocessor Conventions

- Feature toggles use `#if ENABLE_FEATURE` / `#endif` pattern.
- All feature toggles default to `0` (disabled) and are overridable via CMake or compiler flags.
- Platform detection uses compiler-defined macros (`__linux__`, `_WIN32`, `__aarch64__`, etc.).

### Regression Test Baseline

The following test files comprise the project's immutable regression suite. These files MUST NOT be modified under any circumstances. All new tests must be added to new files.

| File | CTest Registration | Type | Scope |
|---|---|---|---|
| `test/vvencinterfacetest/vvencinterfacetest.c` | `Test_vvencinterfacetest` | C API | encoder create/open/encode/close lifecycle |
| `test/vvenclibtest/vvenclibtest.cpp` | `Test_vvenclibtest-parameter_range`<br>`Test_vvenclibtest-calling_order`<br>`Test_vvenclibtest-input_params`<br>`Test_vvenclibtest-sdk_default`<br>`Test_vvenclibtest-sdk_stringapi_interface`<br>`Test_vvenclibtest-timestamps` | C++ SDK | parameter ranges, calling order, invalid input, default behavior, string API, timestamps |
| `test/vvenc_unit_test/vvenc_unit_test.cpp` | `Test_vvenc_unit_test` | C++ unit | ALF, MCTF, RdCost, IntraPrediction, InterPrediction, DepQuant, TrQuant |
| `cmake/modules/vvencTests.cmake` | `Test_vvencapp-*`, `Test_vvencFFapp-*`, `Test_compare_output-*`, `Cleanup_remove_temp_files` | Integration | encoder presets, 2-pass RC, stats files, low-delay, scalar SIMD, output comparison |

When adding tests:
1. Create new `.cpp` or `.c` files in the appropriate `test/` subdirectory.
2. Register them via `add_test()` in the corresponding `CMakeLists.txt`.
3. Never add tests to or modify any file listed in the table above.
