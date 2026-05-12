# MLTools — LightGBM-based ML inference module

if( VVENC_ENABLE_ML_LIGHTGBM )
    add_library( vvenc_ml STATIC
        FASTSplitPredictor.cpp
        CUFeatureExtractor.cpp
        FakeModelFactory.cpp
    )

    target_include_directories( vvenc_ml
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    )

    target_link_libraries( vvenc_ml PUBLIC LightGBM::LightGBM )
    target_compile_definitions( vvenc_ml PUBLIC VVENC_ENABLE_ML_LIGHTGBM=1 )

    set_target_properties( vvenc_ml PROPERTIES FOLDER lib )
endif()
