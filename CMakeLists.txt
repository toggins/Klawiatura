cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(Klawiatura VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(BUILD_SHARED_LIBS OFF)

# Libraries
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
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.20)

# SDL_image (3.2.4)
set(SDLIMAGE_AVIF OFF CACHE INTERNAL "")
set(SDLIMAGE_TIF OFF CACHE INTERNAL "")
set(SDLIMAGE_WEBP OFF CACHE INTERNAL "")
set(SDLIMAGE_SVG OFF CACHE INTERNAL "")
set(SDLIMAGE_XCF OFF CACHE INTERNAL "")
FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.4)

# OpenGL
find_package(OpenGL REQUIRED)

# GLAD (2.0.8, OpenGL 3.3 Core)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake)

# cglm
set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON CACHE INTERNAL "")
FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG v0.9.6)

# FMOD Core API (2.03.08)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/fmod)

# S_fixed
set(S_FIXED_BUILD_TEST OFF CACHE INTERNAL "")
FetchContent_Declare(
    S_fixed
    GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG 0eceb7db9a8f4b501cf3d9c9ab656b008ab82ddf)

# S_tructures
set(S_TRUCTURES_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_Declare(
    S_tructures
    GIT_REPOSITORY https://github.com/Schwungus/S_tructures.git
    GIT_TAG d0825e2b08d1ec66ec4bafd5f7e0cbc74ef95f5d)

# NutPunch
set(NUTPUNCH_BUILD_TESTS OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_SERVER OFF CACHE INTERNAL "")
FetchContent_Declare(
    nutpunch
    GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG 84f1ec28e7deb05f1077b402188b53a4f360ec70)

# yyjson (0.12.0)
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.12.0)

# GekkoNet
set(NO_ASIO_BUILD ON CACHE INTERNAL "")
FetchContent_Declare(
    GekkoNet
    GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG cc7318f8088ce2561f3adcb055f3d09a2e919f76)

# zlib
set(ZLIB_BUILD_TESTING OFF CACHE INTERNAL "")
set(ZLIB_BUILD_SHARED OFF CACHE INTERNAL "")
set(ZLIB_BUILD_MINIZIP OFF CACHE INTERNAL "")
set(ZLIB_INSTALL OFF CACHE INTERNAL "")
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1)

FetchContent_MakeAvailable(SDL3 SDL3_image glad cglm S_fixed S_tructures nutpunch yyjson GekkoNet zlib)

set(LIBS
    SDL3::SDL3-static SDL3_image::SDL3_image-static
    ${OPENGL_gl_LIBRARY} glad_gl_core_33
    cglm fmod  GekkoNet
    S_fixed S_tructures nutpunch
    yyjson zlibstatic)
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(Klawiatura ${SOURCES} ${HEADERS} ${CMAKE_SOURCE_DIR}/src/Klawiatura.rc)
target_include_directories(Klawiatura PRIVATE ${SOURCE_DIR})
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura PUBLIC -D_POSIX_C_SOURCE=200112L)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo) # you're sus my guy...
    target_compile_options(GekkoNet PRIVATE /fsanitize=address)
endif()

add_custom_target(
    KlawiaturaPost ALL
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Klawiatura> $<TARGET_RUNTIME_DLLS:Klawiatura>
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/modsrc $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND_EXPAND_LISTS)
add_dependencies(KlawiaturaPost Klawiatura)
