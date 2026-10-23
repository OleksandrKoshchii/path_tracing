cmake_minimum_required(VERSION 3.28)
project(VulkanPathTracer
    VERSION 1.0.0
    DESCRIPTION "Advanced Vulkan-based GPU Path Tracer using Slang and Ray Query"
    LANGUAGES CXX
)


# ==============================================================================
# 1. GLOBAL OPTIONS OF COMPILERS AND THEIR STANDARTS
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Turns off compilers' extensions (GNU/MSVC). Clear C++.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# allows IDE to use directories to group files
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
    
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")


endif()

option(ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(Vulkan 1.3 REQUIRED)
if(WIN32)

    set(GLFW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include")
    
    set(GLFW_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib-vc2026/glfw3.lib")
    
    add_library(glfw INTERFACE)
    target_include_directories(glfw INTERFACE ${GLFW_INCLUDE_DIR})
    target_link_libraries(glfw INTERFACE ${GLFW_LIBRARY})
    

    add_library(glfw3::glfw ALIAS glfw)
    set(GLFW3_FOUND TRUE)
    message(STATUS "Native Windows GLFW3 mapped successfully from lib-vc2026")
else()

    find_package(glfw3 REQUIRED)
endif()
if(WIN32)

    set(GLM_INCLUDE_DIR "C:/VulkanSDK/1.4.357.0/include")
    

    add_library(glm INTERFACE)
    target_include_directories(glm INTERFACE ${GLM_INCLUDE_DIR})
    

    add_library(glm::glm ALIAS glm)
    set(GLM_FOUND TRUE)
    message(STATUS "GLM math headers mapped manually from Vulkan SDK include path")
else()
    find_package(glm REQUIRED)
endif()

find_program(SLANGC_EXECUTABLE slangc 
    HINTS "$ENV{VULKAN_SDK}/bin"
    DOC "Path to the Slang shader compiler"
)
if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "Could not find 'slangc' compiler. Please ensure Vulkan SDK environment is set up properly.")
endif()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")


set(PROJECT_SOURCES
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/path_tracer.cpp"
    "${SRC_DIR}/vulkan_context_init.cpp"
    "${SRC_DIR}/vulkan_context_loop.cpp"
    "${SRC_DIR}/geometry/obj_loader.cpp"
)


add_executable(path_tracer ${PROJECT_SOURCES})


target_include_directories(path_tracer PRIVATE 
    ${SRC_DIR}
    /usr/include/stb
)


target_link_libraries(path_tracer PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm
)

target_compile_definitions(path_tracer PRIVATE 
    ASSETS_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/"
    SHADERS_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/"
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS=1
)


if(MSVC)
    target_compile_options(path_tracer PRIVATE /W4 /permissive-)
    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(path_tracer PRIVATE /WX)
    endif()
else() # GCC or Clang
    target_compile_options(path_tracer PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(path_tracer PRIVATE -Werror)
    endif()
endif()


set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

file(GLOB SHADERS_SRC CONFIGURE_DEPENDS "${SHADERS_DIR}/*.slang")

function(add_shader_compilation_command SHADER_NAME)

    set(SHADER_SRC_WITH_EXT "${SHADER_NAME}.slang")
    set(SHADER_BYTECODE_WITH_EXT "${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${SHADER_BYTECODE_WITH_EXT}"
        COMMAND ${SLANGC_EXECUTABLE} "${SHADERS_DIR}/${SHADER_SRC_WITH_EXT}" -target spirv -profile spirv_1_4 -emit-spirv-directly -o "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${SHADER_BYTECODE_WITH_EXT}"
        DEPENDS "${SHADERS_DIR}/${SHADER_SRC_WITH_EXT}"
        COMMENT "Compiling shader Slang: ${SHADER_NAME}"
    )

    add_custom_target(compile_shader_${SHADER_NAME} ALL DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${SHADER_BYTECODE_WITH_EXT}")

    add_dependencies(path_tracer compile_shader_${SHADER_NAME})
endfunction()

foreach(SHADER_PATH ${SHADERS_SRC})
    get_filename_component(NAME_ONLY ${SHADER_PATH} NAME_WE)

    add_shader_compilation_command(${NAME_ONLY})
endforeach()

if(EXISTS "${ASSETS_DIR}")
    add_custom_command(
        TARGET path_tracer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${ASSETS_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
        COMMENT "Assets-Sync: Copying modified textures and models to build directory..."
    )
endif()

