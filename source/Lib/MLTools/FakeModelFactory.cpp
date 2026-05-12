/** \file     FakeModelFactory.cpp
    \brief    Synthetic LightGBM model generator for testing (implementation)
*/

#include "FakeModelFactory.h"
#include "CUFeatureExtractor.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace vvenc {

const char* FakeModelFactory::MODEL_NAMES[5] = {
    "qt_split_model.txt",
    "bh_split_model.txt",
    "bv_split_model.txt",
    "th_split_model.txt",
    "tv_split_model.txt"
};

int FakeModelFactory::writeDummyModel(const std::string& path,
                                      int numFeatures,
                                      double constantScore)
{
    std::ofstream out(path);
    if (!out.is_open())
        return -1;

    // header (LightGBM v4 format)
    out << "tree\n";
    out << "version=v4\n";
    out << "num_class=1\n";
    out << "num_tree_per_iteration=1\n";
    out << "label_index=0\n";
    out << "max_feature_idx=" << (numFeatures - 1) << "\n";
    out << "objective=binary sigmoid:1\n";
    out << "feature_names=";
    for (int i = 0; i < numFeatures; ++i)
    {
        if (i > 0) out << " ";
        out << "feat_" << i;
    }
    out << "\n";

    out << "feature_infos=";
    for (int i = 0; i < numFeatures; ++i)
    {
        if (i > 0) out << " ";
        out << "[0:1]";
    }
    out << "\n\n";

    // single-leaf tree
    out << "Tree=0\n";
    out << "num_leaves=1\n";
    out << "num_cat=0\n";
    out << "split_feature=\n";
    out << "threshold=\n";
    out << "decision_type=\n";
    out << "left_child=\n";
    out << "right_child=\n";
    // leaf_value is raw (before sigmoid). For binary classification,
    // output = sigmoid(leaf_value). Use 0.0 to get exactly 0.5 after transform.
    double rawValue = (constantScore == 0.5) ? 0.0 : constantScore;
    out << "leaf_value=" << rawValue << "\n";
    out << "leaf_count=100000\n";
    out << "internal_value=" << rawValue << "\n";
    out << "internal_count=100000\n";
    out << "is_linear=0\n";
    out << "shrinkage=1\n\n";

    out << "feature_importances:\n";
    for (int i = 0; i < numFeatures; ++i)
        out << "feat_" << i << "=1\n";

    out.close();
    return 0;
}

int FakeModelFactory::writeAllDummyModels(const std::string& outputDir,
                                          double constantScore)
{
    for (int i = 0; i < 5; ++i)
    {
        std::string path = outputDir + "/" + MODEL_NAMES[i];
        if (writeDummyModel(path, CUFeatureExtractor::NUM_FEATURES, constantScore) != 0)
            return -1;
    }
    return 0;
}

}
