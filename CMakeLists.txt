cmake_minimum_required(VERSION 3.28.0 FATAL_ERROR)

project(Klawiatura VERSION 1.4.0
    DESCRIPTION "Mario Forever Online"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(BUILD_SHARED_LIBS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(EMSCRIPTEN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

include(CheckIPOSupported)
check_ipo_supported(RESULT IS_IPO_SUPPORTED)

# SDL3 (3.4.14)
set(SDL_EMSCRIPTEN_PERSISTENT_PATH /storage CACHE INTERNAL "")
set(SDL_SHARED OFF CACHE INTERNAL "")
set(SDL_STATIC ON CACHE INTERNAL "")
set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "")
set(SDL_RENDER OFF CACHE INTERNAL "")
set(SDL_AUDIO ON CACHE INTERNAL "")
set(SDL_GPU OFF CACHE INTERNAL "")
set(SDL_CAMERA OFF CACHE INTERNAL "")
set(SDL_INSTALL ON CACHE INTERNAL "")

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SDL3)

list(APPEND LIBS SDL3::SDL3-static)

# SDL_mixer (3.2.4)
set(SDL3_MIXER_INSTALL ON CACHE INTERNAL "")

set(SDLMIXER_WAVE ON CACHE INTERNAL "")
set(SDLMIXER_AIFF OFF CACHE INTERNAL "")
set(SDLMIXER_VOC OFF CACHE INTERNAL "")
set(SDLMIXER_AU OFF CACHE INTERNAL "")
set(SDLMIXER_WAVPACK OFF CACHE INTERNAL "")
set(SDLMIXER_OPUS OFF CACHE INTERNAL "")
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

set(SDLMIXER_MIDI ON CACHE INTERNAL "")
set(SDLMIXER_MIDI_FLUIDSYNTH ON CACHE INTERNAL "")
set(SDLMIXER_MIDI_FLUIDSYNTH_SHARED OFF CACHE INTERNAL "")
set(SDLMIXER_MIDI_TIMIDITY OFF CACHE INTERNAL "")

set(SDLMIXER_VENDORED ON CACHE INTERNAL "")

if(EMSCRIPTEN)
    set(osal "embedded" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG 9c12eb245ebbf568b6eb7b838cf350aa9f729497
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SDL3_mixer)

list(APPEND LIBS SDL3_mixer::SDL3_mixer-static)

# SDL_GameControllerDB
FetchContent_Declare(SDL_GameControllerDB
    GIT_REPOSITORY https://github.com/mdqinc/SDL_GameControllerDB.git
    GIT_TAG 92580540a27913da37a34cfcc006f973d471c081
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SDL_GameControllerDB)

if(EMSCRIPTEN)
    set(GLAD_VARIANT gles2)
else()
    set(GLAD_VARIANT gl)
endif()

add_library(glad INTERFACE ${CMAKE_SOURCE_DIR}/external/glad-${GLAD_VARIANT}/glad.h)
target_include_directories(glad INTERFACE ${CMAKE_SOURCE_DIR}/external/glad-${GLAD_VARIANT})
list(APPEND LIBS glad)

# cglm
set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON CACHE INTERNAL "")

FetchContent_Declare(cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG v0.9.6
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(cglm)

list(APPEND LIBS cglm)

# yyjson (0.12.0)
FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.12.0
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(yyjson)

list(APPEND LIBS yyjson)

# S_fixed
set(S_FIXED_BUILD_TEST OFF CACHE INTERNAL "")

FetchContent_Declare(S_fixed
    GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG c80c5e96df051da9902bdd21afcc9aa09dbb0d40
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(S_fixed)

list(APPEND LIBS S_fixed)

# S_tructures
set(S_TRUCTURES_BUILD_TESTS OFF CACHE INTERNAL "")

FetchContent_Declare(S_tructures
    GIT_REPOSITORY https://github.com/Schwungus/S_tructures.git
    GIT_TAG 60d6adcf4e6a0fbf5cb66fee9a0f3776dc693d72
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(S_tructures)

list(APPEND LIBS S_tructures)

# NutBlast
FetchContent_Declare(NutBlast
	GIT_REPOSITORY https://github.com/Schwungus/NutBlast.git
	GIT_TAG 66c6c18dba1729f1e215849fc65a76c9f48bef5b
	EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(NutBlast)

list(APPEND LIBS NutBlast)

# GekkoNet
set(NO_ASIO_BUILD ON CACHE INTERNAL "")

FetchContent_Declare(GekkoNet
    GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG v20260804094329-5924b5c
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(GekkoNet)

list(APPEND LIBS GekkoNet)

# Discord Social SDK (1.9.15332)
option(KLAW_DISCORD "Use Discord Social SDK" ON)

if(KLAW_DISCORD)
    message(NOTICE "Adding Discord integration")
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/discord EXCLUDE_FROM_ALL SYSTEM)
else()
    message(NOTICE "Skipping Discord integration (set KLAW_DISCORD to use it!)")
endif()

if(KLAW_DISCORD_FR)
    list(APPEND LIBS discord)
endif()

# Dear Bindings/Dear ImGui (v1.92.9)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/imgui EXCLUDE_FROM_ALL SYSTEM)
list(APPEND LIBS imgui)

# Build

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/include/*.h)

set(CONFIGURE_FILE ${CMAKE_BINARY_DIR}/generated/K_cmake.h)
configure_file(${CMAKE_SOURCE_DIR}/include/K_cmake.h.in ${CONFIGURE_FILE} @ONLY)
list(APPEND HEADERS ${CONFIGURE_FILE})

set(CONTROLLER_DB ${sdl_gamecontrollerdb_SOURCE_DIR}/gamecontrollerdb.txt)

set(RESFILE ${CMAKE_SOURCE_DIR}/res/${PROJECT_NAME}.rc)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESFILE})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Release AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections) # -Wl,--icf=all
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping release binary")
endif()

if(IS_IPO_SUPPORTED)
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "LTO enforced for Release builds!")
else()
    message(STATUS "LTO is not supported :(")
endif()

if(KLAW_DISCORD_FR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE K_DISCORD=1)
endif()

set(RUNTIME_DLLS "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>" CACHE INTERNAL "")
function(append_dll TARGET)
    set(DLL "$<TARGET_PROPERTY:${TARGET},IMPORTED_LOCATION>")
    set(RUNTIME_DLLS "$<LIST:APPEND,${RUNTIME_DLLS},${DLL}>" CACHE INTERNAL "")
endfunction()

if(LINUX)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-rpath,./)
    if(KLAW_DISCORD_FR)
        append_dll(discord)
    endif()
endif()

if(EMSCRIPTEN)
    target_link_options(${PROJECT_NAME} PRIVATE
        "SHELL:-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAPU32,HEAPU8"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@"
        "SHELL:--preload-file ${CONTROLLER_DB}@gamecontrollerdb.txt")
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${RUNTIME_DLLS} ${CONTROLLER_DB}

        COMMAND ${CMAKE_COMMAND} -E remove_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/data

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:${PROJECT_NAME}>/

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
