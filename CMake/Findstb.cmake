find_path(stb_INCLUDE_DIR NAMES stb_image.h PATH_SUFFIXES stb)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(stb DEFAULT_MSG stb_INCLUDE_DIR)

if(stb_FOUND)
    set(stb_INCLUDE_DIRS ${stb_INCLUDE_DIR})
endif()

