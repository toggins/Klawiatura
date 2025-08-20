cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(
    Klawiatura
    VERSION 0.1.0
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Libraries
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.2.16)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_RENDER OFF)
set(SDL_GPU OFF)
set(SDL_AUDIO OFF)
set(SDL_CAMERA OFF)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.16)
FetchContent_MakeAvailable(SDL3)

# SDL_image (3.2.4)
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_XCF OFF)

FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.4)
FetchContent_MakeAvailable(SDL3_image)

# OpenGL
find_package(OpenGL REQUIRED)

# GLAD (2.0.8, OpenGL 3.3 Core)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)

# cglm
set(CGLM_SHARED OFF)
set(CGLM_STATIC ON)

FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG v0.9.6)
FetchContent_MakeAvailable(cglm)

# FMOD Core API (2.03.08)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/fmod)

# GekkoNet
FetchContent_Declare(
    GekkoNet
    GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG 5e10f41b107d33c2147a08c9ce214670f324e9d1)
FetchContent_MakeAvailable(GekkoNet)

# TODO: should probably just send a PR for this...
file(GLOB GEKKO_SRC ${gekkonet_SOURCE_DIR}/GekkoLib/src/*.cpp)
add_library(GekkoNet STATIC ${GEKKO_SRC})
target_compile_definitions(GekkoNet PUBLIC GEKKONET_STATIC GEKKONET_NO_ASIO)
target_include_directories(
    GekkoNet
    PUBLIC ${gekkonet_SOURCE_DIR}/GekkoLib/include
    PRIVATE ${gekkonet_SOURCE_DIR}/GekkoLib/thirdparty)

# S_fixed
set(S_FIXED_BUILD_TEST OFF)

FetchContent_Declare(
    S_fixed
    GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG master)
FetchContent_MakeAvailable(S_fixed)

set(NUTPUNCH_BUILD_TESTS OFF)
set(NUTPUNCH_BUILD_SERVER OFF)

FetchContent_Declare(
    nutpunch
    GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG master)
FetchContent_MakeAvailable(nutpunch)

set(LIBS
    SDL3::SDL3-static
    SDL3_image::SDL3_image-static
    ${OPENGL_gl_LIBRARY}
    glad_gl_core_33
    cglm
    fmod
    GekkoNet
    S_fixed
    nutpunch)

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(Klawiatura ${SOURCES} ${HEADERS})
target_include_directories(Klawiatura PRIVATE ${SOURCE_DIR})
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura PUBLIC -D_POSIX_C_SOURCE=200112L)

add_custom_command(
    TARGET Klawiatura
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Klawiatura>
            $<TARGET_FILE_DIR:Klawiatura>
    COMMAND_EXPAND_LISTS
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/modsrc
            $<TARGET_FILE_DIR:Klawiatura>/data)
