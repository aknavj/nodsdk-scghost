# src/viewer/CMakeLists.txt — Standalone SCG Asset Browser
#
# Layered architecture:
#   core/      — pure data: AssetDatabase, Strings
#   render/    — GL: Shaders, Camera, TextureUpload, PreviewRenderer
#   scene/     — DDS catalog, materials, NIL entity instancing, builders
#   animation/ — bone evaluator, animation controller
#   ui/        — Inspectors + DetailsPanels (ImGui)
#   app/       — BrowserView (controller) + ViewerApp (platform shell)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 3.3 QUIET)
find_package(GLEW QUIET)

if(WIN32)
    find_path(DIRENT_INCLUDE_DIR dirent.h)
    if(NOT DIRENT_INCLUDE_DIR)
        message(FATAL_ERROR "Windows viewer requires dirent.h; configure with the vcpkg toolchain.")
    endif()
    set(NODSDK_AUDIO_DEFAULT OFF)
else()
    set(NODSDK_AUDIO_DEFAULT ON)
endif()
option(NODSDK_ENABLE_AUDIO "Enable POSIX audio playback using aplay" ${NODSDK_AUDIO_DEFAULT})
if(WIN32 AND NODSDK_ENABLE_AUDIO)
    message(FATAL_ERROR "Windows audio playback is not supported. Set NODSDK_ENABLE_AUDIO=OFF.")
endif()

if(NOT glfw3_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 IMPORTED_TARGET glfw3)
    endif()
endif()

# ─── Dear ImGui (vendored, reuse from parsers/third_party) ──────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/src/parsers/third_party/imgui)
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "ImGui is missing. Run git submodule update --init --recursive.")
endif()

if(NOT TARGET imgui_browser)
    add_library(imgui_browser STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_browser PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_browser PUBLIC OpenGL::GL)
    if(glfw3_FOUND)
        target_link_libraries(imgui_browser PUBLIC glfw)
    elseif(TARGET PkgConfig::GLFW3)
        target_link_libraries(imgui_browser PUBLIC PkgConfig::GLFW3)
    else()
        target_link_libraries(imgui_browser PUBLIC glfw)
    endif()
endif()

# ─── Asset Browser executable ────────────────────────────────────────
add_executable(nodsdk-viewer
    main.cpp

    AudioPlayer.cpp

    # core/
    core/Strings.cpp
    core/AssetDatabase.cpp

    # render/
    render/Shaders.cpp
    render/Camera.cpp
    render/TextureUpload.cpp
    render/PreviewRenderer.cpp

    # scene/
    scene/DdsCatalog.cpp
    scene/MaterialLibrary.cpp
    scene/EntityInstancer.cpp
    scene/PreviewBuilders.cpp

    # animation/
    animation/BoneEvaluator.cpp
    animation/AnimationController.cpp

    # ui/
    ui/Inspectors.cpp
    ui/DetailsPanels.cpp

    # app/
    app/BrowserView.cpp
    app/ViewerApp.cpp
)

target_include_directories(nodsdk-viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

if(WIN32)
    target_include_directories(nodsdk-viewer PRIVATE ${DIRENT_INCLUDE_DIR})
    target_compile_definitions(nodsdk-viewer PRIVATE NOMINMAX)
endif()
if(NOT NODSDK_ENABLE_AUDIO)
    target_compile_definitions(nodsdk-viewer PRIVATE NODSDK_AUDIO_DISABLED)
    message(STATUS "Viewer audio playback disabled; audio inspection remains available")
endif()

target_link_libraries(nodsdk-viewer PRIVATE
    asset_parsers
    imgui_browser
    OpenGL::GL
    Threads::Threads
)

if(glfw3_FOUND)
    target_link_libraries(nodsdk-viewer PRIVATE glfw)
elseif(TARGET PkgConfig::GLFW3)
    target_link_libraries(nodsdk-viewer PRIVATE PkgConfig::GLFW3)
else()
    target_link_libraries(nodsdk-viewer PRIVATE glfw)
endif()

if(GLEW_FOUND)
    target_link_libraries(nodsdk-viewer PRIVATE GLEW::GLEW)
else()
    target_link_libraries(nodsdk-viewer PRIVATE GLEW)
endif()

if(BUILD_TESTING AND NOT NODSDK_ENABLE_AUDIO)
    add_executable(nodsdk-audio-disabled-tests
        ${CMAKE_SOURCE_DIR}/tests/audio_disabled.cpp
        AudioPlayer.cpp
    )
    target_include_directories(nodsdk-audio-disabled-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(nodsdk-audio-disabled-tests PRIVATE NODSDK_AUDIO_DISABLED)
    target_link_libraries(nodsdk-audio-disabled-tests PRIVATE Threads::Threads)
    add_test(NAME viewer-audio-disabled COMMAND nodsdk-audio-disabled-tests)
endif()
