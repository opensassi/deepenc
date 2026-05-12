# FakeModelFactory — Synthetic LightGBM Model Generator for Testing

## 1. Overview

`FakeModelFactory` is a test utility class that generates synthetic LightGBM native `.txt` model files. These produce a constant prediction score (e.g., 0.5) for any input, enabling unit testing of `FASTSplitPredictor` without requiring a real training pipeline.

## 2. Component Specifications

```cpp
#pragma once

#include <string>

namespace vvenc {

class FakeModelFactory {
public:
    /** \brief Write a dummy LightGBM model file that always outputs constantScore
     *  \param[in] path           Full output file path
     *  \param[in] numFeatures    Number of features the model expects (default 22)
     *  \param[in] constantScore  Constant prediction output (default 0.5)
     *  \retval 0  File written successfully
     *  \retval -1 File write failed
     */
    static int writeDummyModel(const std::string& path,
                               int numFeatures = 22,
                               double constantScore = 0.5);

    /** \brief Write all 5 dummy models for a full test suite
     *  \param[in] outputDir Directory to write model files into
     *  \param[in] constantScore Constant prediction output (default 0.5)
     *  \retval 0  All files written
     *  \retval -1 One or more files failed
     */
    static const char* MODEL_NAMES[5];

    static int writeAllDummyModels(const std::string& outputDir,
                                   double constantScore = 0.5);
};

}

### LightGBM Native Format

The generated `.txt` file conforms to the LightGBM v4 model text format:

```
tree
version=v4
num_class=1
num_tree_per_iteration=1
label_index=0
max_feature_idx=21
objective=binary sigmoid:1
feature_names=feat_0 feat_1 ... feat_21
feature_infos=[0:1] [0:1] ... [0:1]
tree_sizes=1

Tree=0
num_leaves=1
num_cat=0
split_feature=
threshold=
decision_type=
left_child=
right_child=
leaf_value=0.5
leaf_count=100000
internal_value=0.5
internal_count=100000
is_linear=0
shrinkage=1

feature_importances:
feat_0=1
```

A single-leaf tree with constant leaf_value produces the same output for any input.

## 3-7. (Not applicable — test utility only)

See `MLTools.spec.md` §6 for test coverage of `FakeModelFactory`.
