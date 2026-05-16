if( VVENC_ENABLE_HW_PREANALYSIS )
    add_executable( Test_vvenc_hw_preanalysis hw_preanalysis_test.cpp )
    target_link_libraries( Test_vvenc_hw_preanalysis PRIVATE vvenc_hw )
    add_test( NAME Test_vvenc_hw_preanalysis
              COMMAND Test_vvenc_hw_preanalysis )
    set_target_properties( Test_vvenc_hw_preanalysis PROPERTIES FOLDER test )
endif()
