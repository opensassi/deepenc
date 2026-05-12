# vvenc_mltest — LightGBM ML module unit tests

if( VVENC_ENABLE_ML_LIGHTGBM )
    add_executable( vvenc_mltest
        vvenc_mltest.cpp
    )

    target_link_libraries( vvenc_mltest PRIVATE vvenc_ml vvenc )
    target_include_directories( vvenc_mltest
        PRIVATE ${CMAKE_SOURCE_DIR}/source/Lib
    )

    # Register with CTest
    add_test( NAME Test_vvenc_mltest COMMAND vvenc_mltest )

    set_target_properties( vvenc_mltest PROPERTIES FOLDER test )
endif()
