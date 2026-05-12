# vvenc_mltest — MLTools Module Unit Tests

## Overview

Unit test suite for the MLTools module (FASTSplitPredictor, CUFeatureExtractor, FakeModelFactory). Tests are registered with CTest as `Test_vvenc_mltest` and run using the custom `TEST/TESTT/ERROR` macro pattern with `g_numTests`/`g_numFails` globals.

**Dependencies**: `vvenc_ml` (MLTools static library), `vvenc` (main encoder library).

**Lifecycle**: Each test is a standalone function callable by index from `main()`. The default run (`./vvenc_mltest` with no args) runs all tests sequentially.

## Component Specifications

| Test ID | Function | What it verifies |
|---------|----------|------------------|
| 1 | `testFakeModelFactory_writeSingle` | Single dummy model .txt file is written and contains `leaf_value=0.5` |
| 2 | `testFakeModelFactory_writeAll` | All 5 model files (qt/bh/bv/th/tv) written successfully |
| 3 | `testFASTSplitPredictor_loadAndPredict` | Models load, `isInitialized()` returns true, `predict()` returns confident split |
| 4 | `testFASTSplitPredictor_belowThreshold` | `predict()` with threshold > model confidence returns `NO_SPLIT` |
| 5 | `testFASTSplitPredictor_predictWithoutInit` | `predict()` before `init()` returns error code |
| 6 | `testFASTSplitPredictor_doubleInit` | Double `init()` returns error code |
| 7 | `testFASTSplitPredictor_releaseCycle` | `release()` frees resources, `isInitialized()` returns false |
| 8 | `testFASTSplitPredictor_noModelsDir` | `init()` with nonexistent directory returns error code |
| 9 | `testCUFeatureExtractor_defaultFeatures` | `NUM_FEATURES` constant is 22 |

## 4. Detailed Data Flow

```
main() → for each test:
  test function() → calls MLTools API
  → records pass/fail via g_numTests/g_numFails
main() → prints summary → returns 0 (all pass) or 1 (any fail)
```

## 5. Testing Requirements

The test suite itself tests:
- FASTSplitPredictor lifecycle (init → predict → release)
- Confidence threshold gating
- Error handling for missing models, double init, predict-before-init
- FakeModelFactory output format correctness
- CUFeatureExtractor feature count

## 6. CLI Entry Point

```
./vvenc_mltest             # Run all tests
./vvenc_mltest 3           # Run only test ID 3
```

CTest invocation:
```
ctest -R Test_vvenc_mltest
```
