cmake_minimum_required(VERSION 3.28.0 FATAL_ERROR)

project(Klawiatura VERSION 1.4.0
    DESCRIPTION "Mario Forever Online"
    LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.4.10)

set(SDL_EMSCRIPTEN_PERSISTENT_PATH /storage CACHE INTERNAL "")
set(SDL_SHARED OFF CACHE INTERNAL "")
set(SDL_STATIC ON CACHE INTERNAL "")
set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "")
set(SDL_RENDER OFF CACHE INTERNAL "")
set(SDL_AUDIO ON CACHE INTERNAL "")
set(SDL_GPU OFF CACHE INTERNAL "")
set(SDL_CAMERA OFF CACHE INTERNAL "")

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(SDL3)
list(APPEND LIBS SDL3::SDL3-static)

# Fluidsynth

FetchContent_Declare(Fluidsynth
    GIT_REPOSITORY https://github.com/Schwungus/fluidsynth-emscripten.git
    GIT_TAG 279999696528f69a08df53a9ec7505a0bc9b6b33
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(Fluidsynth)

# SDL_mixer (3.2.4)

set(SDLMIXER_WAVE ON CACHE INTERNAL "")
set(SDLMIXER_AIFF OFF CACHE INTERNAL "")
set(SDLMIXER_VOC OFF CACHE INTERNAL "")
set(SDLMIXER_AU OFF CACHE INTERNAL "")
set(SDLMIXER_WAVPACK OFF CACHE INTERNAL "")
set(SDLMIXER_OPUS OFF CACHE INTERNAL "")
set(SDLMIXER_MIDI ON CACHE INTERNAL "")
set(SDLMIXER_GME OFF CACHE INTERNAL "")

set(SDLMIXER_OGG ON CACHE INTERNAL "")
set(SDLMIXER_VORBIS_STB ON CACHE INTERNAL "")
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE INTERNAL "")
set(SDLMIXER_VORBIS_TREMOR OFF CACHE INTERNAL "")

set(SDLMIXER_FLAC ON CACHE INTERNAL "")
set(SDLMIXER_FLAC_LIBFLAC OFF CACHE INTERNAL "")
set(SDLMIXER_FLAC_DRFLAC OFF CACHE INTERNAL "")
set(SDLMIXER_FLAC_SHARED OFF CACHE INTERNAL "")

set(SDLMIXER_MOD ON CACHE INTERNAL "")
set(SDLMIXER_MOD_XMP ON CACHE INTERNAL "")
set(SDLMIXER_MOD_XMP_LITE OFF CACHE INTERNAL "")
set(SDLMIXER_MOD_XMP_SHARED OFF CACHE INTERNAL "")

set(SDLMIXER_MP3 ON CACHE INTERNAL "")
set(SDLMIXER_MP3_DRMP3 ON CACHE INTERNAL "")
set(SDLMIXER_MP3_MPG123 OFF CACHE INTERNAL "")

set(SDLMIXER_MIDI_FLUIDSYNTH ON CACHE INTERNAL "")
set(SDLMIXER_MIDI_FLUIDSYNTH_SHARED OFF CACHE INTERNAL "")
set(SDLMIXER_MIDI_TIMIDITY OFF CACHE INTERNAL "")

set(SDLMIXER_VENDORED ON CACHE INTERNAL "")

FetchContent_Declare(SDL3_mixer
    GIT_REPOSITORY https://github.com/Schwungus/SDL_mixer.git
    GIT_TAG fe482961efb2f6e51910126b8ae0677f11d74254
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(SDL3_mixer)
list(APPEND LIBS SDL3_mixer::SDL3_mixer-static)

# SDL_GameControllerDB

FetchContent_Declare(SDL_GameControllerDB
    GIT_REPOSITORY https://github.com/mdqinc/SDL_GameControllerDB.git
    GIT_TAG 513c72e34569e0f471dde7aa26eecb23946c3ef7
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(SDL_GameControllerDB)

# GLAD (2.0.8, OpenGL 3.3 Core OR OpenGL ES 3.0)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(glad)

# cglm

set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON CACHE INTERNAL "")

FetchContent_Declare(cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG v0.9.6
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(cglm)
list(APPEND LIBS cglm)

# yyjson (0.12.0)

FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.12.0
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(yyjson)
list(APPEND LIBS yyjson)

# S_fixed

set(S_FIXED_BUILD_TEST OFF CACHE INTERNAL "")

FetchContent_Declare(S_fixed
    GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG 70b3044b3d758108828e2054683c21faf744b3de
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(S_fixed)
list(APPEND LIBS S_fixed)

# S_tructures

set(S_TRUCTURES_BUILD_TESTS OFF CACHE INTERNAL "")

FetchContent_Declare(S_tructures
    GIT_REPOSITORY https://github.com/Schwungus/S_tructures.git
    GIT_TAG e296660eae2b8698e07b550093cb3d7d705a736c
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(S_tructures)
list(APPEND LIBS S_tructures)

# NutBlast

FetchContent_Declare(NutBlast
	GIT_REPOSITORY https://github.com/Schwungus/NutBlast.git
	GIT_TAG 5813fa00492b649644926390f0d3dc325f6a9d98
	EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(NutBlast)
list(APPEND LIBS NutBlast)

# GekkoNet

set(NO_ASIO_BUILD ON CACHE INTERNAL "")

FetchContent_Declare(GekkoNet
    GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG v20260629200724-02c447c
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(GekkoNet)
list(APPEND LIBS GekkoNet)

if(EMSCRIPTEN)
    glad_add_library(glad_gles2_30 REPRODUCIBLE STATIC API gles2=3.0)
    list(APPEND LIBS glad_gles2_30)
else()
    glad_add_library(glad_gl_core_33 REPRODUCIBLE STATIC API gl:core=3.3)
    list(APPEND LIBS glad_gl_core_33)
endif()

# Discord Social SDK (1.9.15332)
set(KLAW_DISCORD OFF CACHE INTERNAL "Use Discord Social SDK")
add_subdirectory(${CMAKE_SOURCE_DIR}/external/discord)
if(KLAW_DISCORD)
    list(APPEND LIBS discord)
endif()

# Build
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/include/*.h)

set(CONFIGURE_FILE ${CMAKE_BINARY_DIR}/generated/K_cmake.h)
configure_file(${CMAKE_SOURCE_DIR}/include/K_cmake.h.in ${CONFIGURE_FILE} @ONLY)
list(APPEND HEADERS ${CONFIGURE_FILE})

set(CONTROLLER_DB ${sdl_gamecontrollerdb_SOURCE_DIR}/gamecontrollerdb.txt)

set(RESFILE ${CMAKE_SOURCE_DIR}/res/Klawiatura.rc)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESFILE})

if(EMSCRIPTEN)
    target_link_options(${PROJECT_NAME} PRIVATE
        "SHELL:-sALLOW_MEMORY_GROWTH"
        "SHELL:-sASYNCIFY"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAPU32,HEAPU8"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2")
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

if(KLAW_DISCORD)
    target_compile_definitions(${PROJECT_NAME} PRIVATE K_DISCORD=1)
endif()

set(RUNTIME_DLLS "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>" CACHE INTERNAL "")
function(append_dll TARGET)
    set(DLL "$<TARGET_PROPERTY:${TARGET},IMPORTED_LOCATION>")
    set(RUNTIME_DLLS "$<LIST:APPEND,${RUNTIME_DLLS},${DLL}>" CACHE INTERNAL "")
endfunction()

if(LINUX)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-rpath,./)
    if(KLAW_DISCORD)
        append_dll(discord)
    endif()
endif()

if(EMSCRIPTEN)
    target_link_options(${PROJECT_NAME} PRIVATE
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@"
        "SHELL:--preload-file ${CONTROLLER_DB}@gamecontrollerdb.txt")
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${RUNTIME_DLLS} ${CONTROLLER_DB}

        COMMAND ${CMAKE_COMMAND} -E remove_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/data

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/data $<TARGET_FILE_DIR:${PROJECT_NAME}>/data

        COMMAND_EXPAND_LISTS)

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . COMPONENT ${PROJECT_NAME})
    install(FILES ${CONTROLLER_DB} ${RUNTIME_DLLS} DESTINATION . COMPONENT ${PROJECT_NAME})
    install(DIRECTORY assets/ DESTINATION . COMPONENT ${PROJECT_NAME})
endif()

set(CPACK_GENERATOR ZIP)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL ${PROJECT_NAME})
include(CPack)
