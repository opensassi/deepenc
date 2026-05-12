/** \file     FakeModelFactory.h
    \brief    Synthetic LightGBM model generator for testing (header)
*/

#pragma once

#include <string>

namespace vvenc {

class FakeModelFactory {
public:
    static int writeDummyModel(const std::string& path,
                               int numFeatures = 22,
                               double constantScore = 0.5);

    static int writeAllDummyModels(const std::string& outputDir,
                                   double constantScore = 0.5);

    static const char* MODEL_NAMES[5];
};

}
