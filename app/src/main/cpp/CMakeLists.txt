cmake_minimum_required(VERSION 3.22.1)
project(mclg_android LANGUAGES C CXX)

# Android ARM64 host + the original Legacy Edition engine sources.
# Platform backends are excluded here; the Android adapters replace them.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MCLG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(MCLG_WORLD_DIR "${MCLG_ROOT}/Minecraft.World")
set(MCLG_CLIENT_DIR "${MCLG_ROOT}/Minecraft.Client")

# Keep the complete shared world/gameplay implementation in the Android
# native build. Platform-specific console/desktop backends are not compiled.
file(GLOB_RECURSE MCLG_WORLD_SOURCES CONFIGURE_DEPENDS
    "${MCLG_WORLD_DIR}/*.cpp"
)

file(GLOB_RECURSE MCLG_CLIENT_SOURCES CONFIGURE_DEPENDS
    "${MCLG_CLIENT_DIR}/*.cpp"
)

set(MCLG_PLATFORM_EXCLUDES
    "/PS3/"
    "/PS3Extras/"
    "/SPU_Tasks/"
    "/Orbis/"
    "/Durango/"
    "/Windows64/"
    "/Xbox/"
    "/PSVita/"
    "/PSVitaExtras/"
)

set(MCLG_ENGINE_SOURCES)
foreach(source IN LISTS MCLG_WORLD_SOURCES MCLG_CLIENT_SOURCES)
    set(excluded FALSE)
    foreach(platform_path IN LISTS MCLG_PLATFORM_EXCLUDES)
        string(FIND "${source}" "${platform_path}" platform_index)
        if(NOT platform_index EQUAL -1)
            set(excluded TRUE)
            break()
        endif()
    endforeach()
    if(NOT excluded)
        list(APPEND MCLG_ENGINE_SOURCES "${source}")
    endif()
endforeach()

list(LENGTH MCLG_WORLD_SOURCES MCLG_WORLD_SOURCE_COUNT)
list(LENGTH MCLG_ENGINE_SOURCES MCLG_ENGINE_SOURCE_COUNT)
message(STATUS "MCLG: discovered ${MCLG_WORLD_SOURCE_COUNT} world/gameplay translation units")
message(STATUS "MCLG: compiling ${MCLG_ENGINE_SOURCE_COUNT} shared engine translation units")

# The legacy code uses headers from many sibling directories. Export every
# source directory as an include root; the platform compatibility layer is
# added in the next Android porting layer.
# Match directory paths only. A recursive "/*" glob also returns every
# header/source file when LIST_DIRECTORIES is true, bloating AGP's CMake
# model to tens of thousands of include entries and exhausting Gradle's heap.
file(GLOB_RECURSE MCLG_INCLUDE_DIRS LIST_DIRECTORIES true
    "${MCLG_WORLD_DIR}/*/"
    "${MCLG_CLIENT_DIR}/*/"
)
list(APPEND MCLG_INCLUDE_DIRS "${MCLG_WORLD_DIR}" "${MCLG_CLIENT_DIR}")
list(REMOVE_DUPLICATES MCLG_INCLUDE_DIRS)

add_library(mclg_engine STATIC ${MCLG_ENGINE_SOURCES})
target_include_directories(mclg_engine PRIVATE ${MCLG_INCLUDE_DIRS})
target_compile_definitions(mclg_engine PRIVATE
    __ANDROID__
    ANDROID
    MCLG_ANDROID
    _LARGEFILE_SOURCE
    _FILE_OFFSET_BITS=64
)
target_compile_options(mclg_engine PRIVATE
    -Wno-deprecated-declarations
    -Wno-unknown-pragmas
    -Wno-unused-parameter
)

add_library(mclg_android SHARED native-lib.cpp)
find_library(log-lib log)
find_library(gles-lib GLESv3)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(z-lib z)

target_compile_features(mclg_android PRIVATE cxx_std_17)
target_compile_options(mclg_android PRIVATE -Wall -Wextra)
target_link_libraries(mclg_android PRIVATE
    mclg_engine
    ${log-lib}
    ${gles-lib}
    ${android-lib}
    ${egl-lib}
    ${z-lib}
)
