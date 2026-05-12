# FindLightGBM.cmake
# Finds the LightGBM C API library and headers.
#
# Targets:
#   LightGBM::LightGBM - Imported target for LightGBM
#
# Variables:
#   LightGBM_FOUND        - True if LightGBM was found
#   LightGBM_INCLUDE_DIR  - Include directory for LightGBM/c_api.h
#   LightGBM_LIBRARY      - Path to lib_lightgbm

# First try find_path/find_library for system-installed packages
find_path( LightGBM_INCLUDE_DIR
    NAMES LightGBM/c_api.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        /home/pc/lib/python3.12/site-packages/lightgbm/include
)

# The pip-installed library is named lib_lightgbm.so (CMake's find_library
# won't match NAMES="lightgbm" because it would look for liblightgbm.so).
# Check known pip-installed locations directly.
find_library( LightGBM_LIBRARY
    NAMES lightgbm liblightgbm lib_lightgbm
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /opt/homebrew/lib
)

if( NOT LightGBM_LIBRARY )
    foreach( _libpath
        "/home/pc/lib/python3.12/site-packages/lightgbm/lib/lib_lightgbm.so"
        "/usr/local/lib/lib_lightgbm.so"
    )
        if( EXISTS "${_libpath}" )
            get_filename_component( _libdir "${_libpath}" DIRECTORY )
            set( LightGBM_LIBRARY "${_libpath}" CACHE FILEPATH "LightGBM library" )
            break()
        endif()
    endforeach()
endif()

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( LightGBM
    REQUIRED_VARS LightGBM_INCLUDE_DIR LightGBM_LIBRARY
)

if( LightGBM_FOUND AND NOT TARGET LightGBM::LightGBM )
    add_library( LightGBM::LightGBM UNKNOWN IMPORTED )
    set_target_properties( LightGBM::LightGBM PROPERTIES
        IMPORTED_LOCATION             "${LightGBM_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LightGBM_INCLUDE_DIR}"
    )
endif()

mark_as_advanced( LightGBM_INCLUDE_DIR LightGBM_LIBRARY )
