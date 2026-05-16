if( VVENC_ENABLE_SCHEDULER_DISPATCH )
    add_executable( sched_pipeline_bench sched_pipeline_bench.cpp )

    target_include_directories( sched_pipeline_bench
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    )

    target_link_libraries( sched_pipeline_bench PRIVATE vvenc )

    add_test( NAME Test_sched_pipeline_bench
              COMMAND sched_pipeline_bench
              WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
              CONFIGURATIONS Release )

    set_target_properties( sched_pipeline_bench PROPERTIES FOLDER test )
endif()
