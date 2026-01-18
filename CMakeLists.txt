cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(Klawiatura VERSION 0.1.0
    DESCRIPTION "Mario Forever Online"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.4.0)
set(SDL_SHARED OFF CACHE INTERNAL "")
set(SDL_STATIC ON CACHE INTERNAL "")
set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "")
set(SDL_RENDER OFF CACHE INTERNAL "")
set(SDL_GPU OFF CACHE INTERNAL "")
set(SDL_AUDIO OFF CACHE INTERNAL "")
set(SDL_CAMERA OFF CACHE INTERNAL "")
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.0)
FetchContent_MakeAvailable(SDL3)
list(APPEND LIBS SDL3::SDL3-static)

# SDL_image (3.2.6)
set(SDLIMAGE_AVIF OFF CACHE INTERNAL "")
set(SDLIMAGE_TIF OFF CACHE INTERNAL "")
set(SDLIMAGE_WEBP OFF CACHE INTERNAL "")
set(SDLIMAGE_SVG OFF CACHE INTERNAL "")
set(SDLIMAGE_XCF OFF CACHE INTERNAL "")
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.6)
FetchContent_MakeAvailable(SDL3_image)
list(APPEND LIBS SDL3_image::SDL3_image-static)

# SDL_GameControllerDB
FetchContent_Declare(SDL_GameControllerDB
    GIT_REPOSITORY https://github.com/mdqinc/SDL_GameControllerDB.git
    GIT_TAG 01de5cf46ff3679b5378ec7dae365791e632b76a)
FetchContent_MakeAvailable(SDL_GameControllerDB)

# GLAD (2.0.8, OpenGL 3.3 Core)
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 REPRODUCIBLE STATIC API gl:core=3.3)
list(APPEND LIBS glad_gl_core_33)

# cglm
set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON CACHE INTERNAL "")
FetchContent_Declare(cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG v0.9.6)
FetchContent_MakeAvailable(cglm)
list(APPEND LIBS cglm)

# FMOD Core API (2.03.11)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/fmod)
list(APPEND LIBS fmod)

# yyjson (0.12.0)
FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.12.0)
FetchContent_MakeAvailable(yyjson)
list(APPEND LIBS yyjson)

# S_fixed
set(S_FIXED_BUILD_TEST OFF CACHE INTERNAL "")
FetchContent_Declare(S_fixed
    GIT_REPOSITORY https://github.com/Schwungus/S_fixed.git
    GIT_TAG 8171508bb7729d94aa98f6ee391a5f30d84ec328)
FetchContent_MakeAvailable(S_fixed)
list(APPEND LIBS S_fixed)

# S_tructures
set(S_TRUCTURES_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_Declare(S_tructures
    GIT_REPOSITORY https://github.com/Schwungus/S_tructures.git
    GIT_TAG 71c959ad0056e33ce4264714461d5cfb7211fec9)
FetchContent_MakeAvailable(S_tructures)
list(APPEND LIBS S_tructures)

# NutPunch
set(NUTPUNCH_STAGING ON CACHE INTERNAL "")
FetchContent_Declare(NutPunch
    GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG 2a2b997dabf8256d63c3e7e9245f0e20cdfe8677)
FetchContent_MakeAvailable(NutPunch)
list(APPEND LIBS NutPunch)

# GekkoNet
set(NO_ASIO_BUILD ON CACHE INTERNAL "")
FetchContent_Declare(GekkoNet
    GIT_REPOSITORY https://github.com/HeatXD/GekkoNet.git
    GIT_TAG 2bf3cb4ed0a11297c3a002a66b9d650052da62cd)
FetchContent_MakeAvailable(GekkoNet)
list(APPEND LIBS GekkoNet)

# Discord Social SDK (1.7.13152)
set(KLAW_DISCORD OFF CACHE INTERNAL "Use Discord Social SDK")
add_subdirectory(${CMAKE_SOURCE_DIR}/external/discord)
if(KLAW_DISCORD)
    list(APPEND LIBS discord)
endif()

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/include/*.h)

set(CONTROLLER_DB ${sdl_gamecontrollerdb_SOURCE_DIR}/gamecontrollerdb.txt)

set(RESFILE ${CMAKE_SOURCE_DIR}/res/Klawiatura.rc)
add_executable(Klawiatura ${SOURCES} ${HEADERS} ${RESFILE})

target_include_directories(Klawiatura
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${NESTED})
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura
    PUBLIC _POSIX_C_SOURCE=200112L _WIN32_WINNT=0x0501 WINVER=0x0501)

if(KLAW_DISCORD)
    target_compile_definitions(Klawiatura PUBLIC K_DISCORD=1)
endif()

if(WIN32)
    set(FLAG "$<IF:$<BOOL:${MSVC}>,/SUBSYSTEM:WINDOWS,-mwindows>")
    target_link_options(Klawiatura PRIVATE ${FLAG})
endif()

set(RUNTIME_DLLS "$<TARGET_RUNTIME_DLLS:Klawiatura>" CACHE INTERNAL "")
function(append_dll TARGET)
    set(DLL "$<TARGET_PROPERTY:${TARGET},IMPORTED_LOCATION>")
    set(RUNTIME_DLLS "$<LIST:APPEND,${RUNTIME_DLLS},${DLL}>" CACHE INTERNAL "")
endfunction()

if(LINUX)
    target_link_options(Klawiatura PRIVATE -Wl,-rpath,./)
    append_dll(fmod)
    if(KLAW_DISCORD)
        append_dll(discord)
    endif()
endif()

add_custom_command(TARGET Klawiatura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Klawiatura>
    ${RUNTIME_DLLS} ${CONTROLLER_DB}

    COMMAND ${CMAKE_COMMAND} -E remove_directory
    $<TARGET_FILE_DIR:Klawiatura>/data

    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/modsrc $<TARGET_FILE_DIR:Klawiatura>/data

    COMMAND_EXPAND_LISTS)

install(TARGETS Klawiatura RUNTIME DESTINATION . COMPONENT Klawiatura)
install(FILES
    README.md UNLICENSE res/console.bat ${CONTROLLER_DB} ${RUNTIME_DLLS}
    DESTINATION . COMPONENT Klawiatura)
install(DIRECTORY modsrc/ DESTINATION data COMPONENT Klawiatura)
# Has all the README .pngs:
install(DIRECTORY .github/assets
    DESTINATION .github COMPONENT Klawiatura)

set(CPACK_GENERATOR ZIP)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL Klawiatura)
include(CPack)
