cmake_minimum_required(VERSION 3.22.1)
project(mclg_android LANGUAGES C CXX)

# Android ARM64 host + the original Legacy Edition engine sources.
# Keep source discovery constrained to directories that actually contain
# game/client translation units. The repository's engine predates C++17;
# compile it as C++14 so libc++ does not inject std::byte into the global
# namespace used by the original headers. The JNI host may use newer C++.
# The platform/media tree is intentionally excluded from recursive discovery.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MCLG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(MCLG_WORLD_DIR "${MCLG_ROOT}/Minecraft.World")
set(MCLG_CLIENT_DIR "${MCLG_ROOT}/Minecraft.Client")
set(MCLG_COMMON_DIR "${MCLG_CLIENT_DIR}/Common")

# The gameplay/world and main client sources live in their respective roots.
file(GLOB MCLG_WORLD_SOURCES
    "${MCLG_WORLD_DIR}/*.cpp"
)
file(GLOB MCLG_CLIENT_ROOT_SOURCES
    "${MCLG_CLIENT_DIR}/*.cpp"
)

# Common client implementation is split across a small, known set of code
# folders. Enumerating these avoids walking thousands of media/resource dirs.
set(MCLG_COMMON_SOURCE_DIRS
    "${MCLG_COMMON_DIR}"
    "${MCLG_COMMON_DIR}/Audio"
    "${MCLG_COMMON_DIR}/Colours"
    "${MCLG_COMMON_DIR}/DLC"
    "${MCLG_COMMON_DIR}/GameRules"
    "${MCLG_COMMON_DIR}/Leaderboards"
    "${MCLG_COMMON_DIR}/Network"
    "${MCLG_COMMON_DIR}/Network/Sony"
    "${MCLG_COMMON_DIR}/Telemetry"
    "${MCLG_COMMON_DIR}/Trial"
    "${MCLG_COMMON_DIR}/Tutorial"
    "${MCLG_COMMON_DIR}/UI"
    "${MCLG_COMMON_DIR}/XUI"
)

set(MCLG_COMMON_SOURCES)
foreach(source_dir IN LISTS MCLG_COMMON_SOURCE_DIRS)
    file(GLOB directory_sources "${source_dir}/*.cpp")
    list(APPEND MCLG_COMMON_SOURCES ${directory_sources})
endforeach()

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

set(MCLG_ENGINE_SOURCES
    "${MCLG_CLIENT_DIR}/Android/Android_App.cpp"
    "${MCLG_CLIENT_DIR}/Android/Android_Render.cpp"
    "${MCLG_CLIENT_DIR}/Android/Android_Input.cpp"
    "${MCLG_CLIENT_DIR}/Android/Android_QNet.cpp"
    "${MCLG_CLIENT_DIR}/Xbox/Network/NetworkPlayerXbox.cpp"
)
foreach(source IN LISTS MCLG_WORLD_SOURCES MCLG_CLIENT_ROOT_SOURCES MCLG_COMMON_SOURCES)
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

# Header lookup is limited to code directories. Platform SDK/media folders are
# intentionally not added to the compiler search path.
set(MCLG_INCLUDE_DIRS
    "${MCLG_WORLD_DIR}"
    "${MCLG_WORLD_DIR}/x64headers"
    "${MCLG_CLIENT_DIR}/PS3/PS3Extras/DirectX"
    "${MCLG_CLIENT_DIR}"
    "${MCLG_COMMON_DIR}"
    ${MCLG_COMMON_SOURCE_DIRS}
)
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
