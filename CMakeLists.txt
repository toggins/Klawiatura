cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(Klawiatura VERSION 0.1.0
    DESCRIPTION "Mario Forever Online"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.2.24)
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
    GIT_TAG release-3.2.24)

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
    GIT_TAG bc38cdbf86ec87a738c4218e0a705e4afb7eac7e)

# NutPunch
set(NUTPUNCH_BUILD_TESTS OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_SERVER OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_LOBBYIST OFF CACHE INTERNAL "")
set(NUTPUNCH_STAGING ON CACHE INTERNAL "")
FetchContent_Declare(
    NutPunch
    GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG staging)

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
    GIT_TAG 4d250da933b9cdf79eb29aff63613e34ab7bfd32)

# FMOD Core API (2.03.08)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/fmod)

FetchContent_MakeAvailable(SDL3 SDL3_image glad cglm S_fixed S_tructures NutPunch yyjson GekkoNet)
glad_add_library(glad_gl_core_33 REPRODUCIBLE STATIC API gl:core=3.3)

set(LIBS
    SDL3::SDL3-static SDL3_image::SDL3_image-static
    glad_gl_core_33
    cglm fmod  GekkoNet
    S_fixed S_tructures NutPunch
    yyjson)

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/include/*.h)

# Thanks <https://stackoverflow.com/a/47801116>
function(embed_txt FILENAME)
    file(READ ${CMAKE_SOURCE_DIR}/${FILENAME} CONTENT)
    string(REGEX REPLACE [./] _ VARNAME ${FILENAME})
    file(WRITE ${CMAKE_BINARY_DIR}/nested/embeds/${FILENAME}
        "const char* const ${VARNAME} = R\"(${CONTENT})\";")
endfunction()

embed_txt(shaders/vertex.glsl)
embed_txt(shaders/fragment.glsl)

add_executable(Klawiatura ${SOURCES} ${HEADERS} ${CMAKE_SOURCE_DIR}/res/Klawiatura.rc)
target_include_directories(Klawiatura PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/nested)
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura PUBLIC _POSIX_C_SOURCE=200112L _WIN32_WINNT=0x0501 WINVER=0x0501)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo) # you're sus my guy...
    target_compile_options(GekkoNet PRIVATE /fsanitize=address)
endif()
if(WIN32)
    target_link_options(Klawiatura PRIVATE $<IF:$<BOOL:${MSVC}>,/SUBSYSTEM:WINDOWS,-mwindows>)
endif()

set(RUNTIME_DLLS $<TARGET_RUNTIME_DLLS:Klawiatura>)
if(LINUX)
    target_link_options(Klawiatura PRIVATE -Wl,-rpath,./)
    set(RUNTIME_DLLS "$<LIST:APPEND,${RUNTIME_DLLS},$<TARGET_PROPERTY:fmod,IMPORTED_LOCATION>>")
endif()

add_custom_target(KlawiaturaPost ALL
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Klawiatura> ${RUNTIME_DLLS}
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/modsrc $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND_EXPAND_LISTS)
add_dependencies(KlawiaturaPost Klawiatura)

install(TARGETS Klawiatura RUNTIME DESTINATION .) # TODO: exclude lib/ and include/ somehow
install(FILES README.md UNLICENSE res/console.bat ${RUNTIME_DLLS} DESTINATION .)
install(DIRECTORY modsrc/ DESTINATION data)
install(DIRECTORY .github/assets DESTINATION .github) # has all the README .pngs

set(CPACK_GENERATOR ZIP)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
include(CPack)
