cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(Klawiatura VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Libraries
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.2.20)
set(SDL_SHARED OFF CACHE INTERNAL "")
set(SDL_STATIC ON CACHE INTERNAL "")
set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "")
set(SDL_RENDER OFF CACHE INTERNAL "")
set(SDL_GPU OFF CACHE INTERNAL "")
set(SDL_AUDIO OFF CACHE INTERNAL "")
set(SDL_CAMERA OFF CACHE INTERNAL "")

FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                     GIT_TAG release-3.2.20)
FetchContent_MakeAvailable(SDL3)

# SDL_image (3.2.4)
set(SDLIMAGE_AVIF OFF CACHE INTERNAL "")
set(SDLIMAGE_TIF OFF CACHE INTERNAL "")
set(SDLIMAGE_WEBP OFF CACHE INTERNAL "")
set(SDLIMAGE_SVG OFF CACHE INTERNAL "")
set(SDLIMAGE_XCF OFF CACHE INTERNAL "")

FetchContent_Declare(
    SDL3_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.4)
FetchContent_MakeAvailable(SDL3_image)

# OpenGL
find_package(OpenGL REQUIRED)

# GLAD (2.0.8, OpenGL 3.3 Core)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git
                     GIT_TAG v2.0.8 SOURCE_SUBDIR cmake)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)

# cglm
set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON CACHE INTERNAL "")

FetchContent_Declare(cglm GIT_REPOSITORY https://github.com/recp/cglm.git
                     GIT_TAG v0.9.6)
FetchContent_MakeAvailable(cglm)

# FMOD Core API (2.03.08)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/fmod)

# GekkoNet
set(NO_ASIO_BUILD ON CACHE INTERNAL "")
FetchContent_Declare(
    GekkoNet GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG e12c0089b5541c3e07a38c4085363211e341af2f)
FetchContent_MakeAvailable(GekkoNet)

# S_fixed
set(S_FIXED_BUILD_TEST OFF CACHE INTERNAL "")

FetchContent_Declare(
    S_fixed GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG 0eceb7db9a8f4b501cf3d9c9ab656b008ab82ddf)
FetchContent_MakeAvailable(S_fixed)

# S_tructures
set(S_TRUCTURES_BUILD_TESTS OFF CACHE INTERNAL "")

FetchContent_Declare(
    S_tructures GIT_REPOSITORY https://github.com/Schwungus/S_tructures.git
    GIT_TAG master)
FetchContent_MakeAvailable(S_tructures)

# NutPunch
set(NUTPUNCH_BUILD_TESTS OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_SERVER OFF CACHE INTERNAL "")

FetchContent_Declare(
    nutpunch GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG fe079aed194263aa05876d01afdc9548b04bdbb2)
FetchContent_MakeAvailable(nutpunch)

# yyjson (0.12.0)
FetchContent_Declare(yyjson GIT_REPOSITORY https://github.com/ibireme/yyjson.git
                     GIT_TAG 0.12.0)
FetchContent_MakeAvailable(yyjson)

set(LIBS
    SDL3::SDL3-static
    SDL3_image::SDL3_image-static
    ${OPENGL_gl_LIBRARY}
    glad_gl_core_33
    cglm
    fmod
    GekkoNet
    S_fixed
    S_tructures
    nutpunch
    yyjson)

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(Klawiatura ${SOURCES} ${HEADERS}
                          ${CMAKE_SOURCE_DIR}/res/Klawiatura.rc)
target_include_directories(Klawiatura PRIVATE ${SOURCE_DIR})
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura PUBLIC -D_POSIX_C_SOURCE=200112L)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    target_compile_options(GekkoNet PUBLIC /fsanitize=address)
endif()

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
