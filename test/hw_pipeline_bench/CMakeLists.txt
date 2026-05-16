if( VVENC_ENABLE_HW_PREANALYSIS )
    add_executable( hw_pipeline_bench pipeline_bench.cpp )

    target_include_directories( hw_pipeline_bench
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    )

    target_link_libraries( hw_pipeline_bench PRIVATE vvenc )

    add_test( NAME Test_hw_pipeline_bench
              COMMAND hw_pipeline_bench
              WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
              CONFIGURATIONS Release )

    set_target_properties( hw_pipeline_bench PROPERTIES FOLDER test )
endif()
