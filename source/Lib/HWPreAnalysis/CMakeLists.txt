if( VVENC_ENABLE_HW_PREANALYSIS )
    add_library( vvenc_hw STATIC
        HWPreAnalyzer.cpp
        HWBitstreamParser.cpp
        HWCuPartitionAnalyzer.cpp
        HWPreAnalysisStage.cpp
    )

    target_include_directories( vvenc_hw
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    )

    target_compile_definitions( vvenc_hw PUBLIC VVENC_ENABLE_HW_PREANALYSIS=1 )

    set_target_properties( vvenc_hw PROPERTIES FOLDER lib )

    message( STATUS "VVENC_ENABLE_HW_PREANALYSIS: ON" )
else()
    message( STATUS "VVENC_ENABLE_HW_PREANALYSIS: OFF" )
endif()
