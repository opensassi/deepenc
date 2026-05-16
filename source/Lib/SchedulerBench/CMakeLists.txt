# Scheduler bench harness — standalone replay and validation
set( SCHED_BENCH_NAME sched_bench )

set( SCHED_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/SchedulerBench.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TraceLoader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExecutorStubs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Scheduler/RingBuffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Scheduler/TUPipelineDAG.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Scheduler/TUScheduler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Scheduler/SchedulerTrace.cpp"
)

add_executable( ${SCHED_BENCH_NAME} ${SCHED_SRC} )

target_include_directories( ${SCHED_BENCH_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/source/Lib"
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions( ${SCHED_BENCH_NAME} PRIVATE
    ENABLE_SCHEDULER_DISPATCH=1
)

set_target_properties( ${SCHED_BENCH_NAME} PROPERTIES
    FOLDER "tools"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
