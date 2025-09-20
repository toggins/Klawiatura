cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(Klawiatura VERSION 0.1.0
    DESCRIPTION "Mario Forever Online"
    LANGUAGES C CXX)

set(CPACK_PACKAGE_VENDOR toggins)
set(CPACK_PACKAGE_DESCRIPTION "TODO: write")
set(CPACK_GENERATOR ZIP)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(BUILD_SHARED_LIBS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# SDL3 (3.2.22)
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
    GIT_TAG release-3.2.22)

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
    GIT_TAG 9b1aae894a4f592125d1fb50e66787303cbe4a44)

# NutPunch
set(NUTPUNCH_BUILD_TESTS OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_SERVER OFF CACHE INTERNAL "")
set(NUTPUNCH_BUILD_LOBBYIST OFF CACHE INTERNAL "")
FetchContent_Declare(
    nutpunch
    GIT_REPOSITORY https://github.com/Schwungus/nutpunch.git
    GIT_TAG 32f92a86da90f00780f1a32c3f6320db1ea06ee7)

# yyjson (0.12.0)
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.12.0)

# GekkoNet
set(NO_ASIO_BUILD ON CACHE INTERNAL "")
FetchContent_Declare(
    GekkoNet
    GIT_REPOSITORY https://github.com/Schwungus/GekkoNet.git
    GIT_TAG 4242ecb8daa66281517f26ae928f304032e3418e)

# FMOD Core API (2.03.08)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/fmod)

FetchContent_MakeAvailable(SDL3 SDL3_image glad cglm S_fixed S_tructures nutpunch yyjson GekkoNet)
glad_add_library(glad_gl_core_33 REPRODUCIBLE STATIC API gl:core=3.3)

set(LIBS
    SDL3::SDL3-static SDL3_image::SDL3_image-static
    ${OPENGL_gl_LIBRARY} glad_gl_core_33
    cglm fmod  GekkoNet
    S_fixed S_tructures nutpunch
    yyjson)

# Build
file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)

# Thanks <https://stackoverflow.com/a/47801116>
function(embed_txt FILENAME)
    file(READ ${CMAKE_SOURCE_DIR}/src/${FILENAME} CONTENT)
    file(WRITE ${CMAKE_BINARY_DIR}/nested/embeds/${FILENAME} "R\"(${CONTENT})\"")
endfunction()

embed_txt(vertex.glsl)
embed_txt(fragment.glsl)

add_executable(Klawiatura ${SOURCES} ${HEADERS} ${CMAKE_SOURCE_DIR}/src/Klawiatura.rc)
target_include_directories(Klawiatura PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/nested)
target_link_libraries(Klawiatura PRIVATE ${LIBS})
target_compile_definitions(Klawiatura PUBLIC -D_POSIX_C_SOURCE=200112L)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo) # you're sus my guy...
    target_compile_options(GekkoNet PRIVATE /fsanitize=address)
endif()

set(RUNTIME_DLLS $<TARGET_RUNTIME_DLLS:Klawiatura>)
if(LINUX)
    target_link_options(Klawiatura PRIVATE -Wl,-rpath,./)
    set(RUNTIME_DLLS "$<TARGET_PROPERTY:fmod,IMPORTED_LOCATION> ${RUNTIME_DLLS}")
endif()

add_custom_target(KlawiaturaPost ALL
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Klawiatura> ${RUNTIME_DLLS}
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/modsrc $<TARGET_FILE_DIR:Klawiatura>/data
    COMMAND_EXPAND_LISTS)
add_dependencies(KlawiaturaPost Klawiatura)

install(TARGETS Klawiatura RUNTIME DESTINATION .) # TODO: exclude lib/ and include/ somehow
install(DIRECTORY modsrc/ DESTINATION data)
install(FILES README.md UNLICENSE DESTINATION .)
install(DIRECTORY .github/assets DESTINATION .github) # has all the README .pngs
install(FILES ${RUNTIME_DLLS} DESTINATION .)

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
include(CPack)
