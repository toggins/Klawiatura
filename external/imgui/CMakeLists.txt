file(GLOB_RECURSE IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE IMGUI_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(imgui STATIC
    ${IMGUI_SOURCES}
    ${IMGUI_HEADERS}
)

target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

if (NOT EMSCRIPTEN)
    target_link_libraries(imgui PRIVATE SDL3::SDL3-static)
endif()
