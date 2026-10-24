find_package(OpenGL REQUIRED)
if(TARGET imgui_browser)
    return()
endif()

find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 IMPORTED_TARGET glfw3)
    endif()
endif()
if(glfw3_FOUND)
    set(NODSDK_GLFW_TARGET glfw)
elseif(TARGET PkgConfig::GLFW3)
    set(NODSDK_GLFW_TARGET PkgConfig::GLFW3)
else()
    message(FATAL_ERROR "GLFW 3.3 is required for the viewer/editors.")
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/parsers/third_party/imgui)
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "ImGui is missing. Run git submodule update --init --recursive.")
endif()
add_library(imgui_browser STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_browser PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui_browser PUBLIC OpenGL::GL ${NODSDK_GLFW_TARGET})
