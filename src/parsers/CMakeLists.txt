# parsers/CMakeLists.txt — Pure asset parser library (no GL dependencies)

find_package(glm QUIET)

set(PARSER_SOURCES
    AssetParser.cpp
    DDSParser.cpp
    NADParser.cpp
    NODParser.cpp
    NMSParser.cpp
    TerrainParser.cpp
    NILParser.cpp
    NLFXParser.cpp
    NPDParser.cpp
    NSAParser.cpp
    NSDParser.cpp
    NODProperties.cpp
    XPRParser.cpp
    NLTParser.cpp
    TGAParser.cpp
    NOCParser.cpp
    NLSParser.cpp
    NOTParser.cpp
    NOTDatabase.cpp
    NFXParser.cpp
    NUIParser.cpp
    NAKParser.cpp
    NCEParser.cpp
    NCSParser.cpp
    NCOParser.cpp
    NMBParser.cpp
    NNBParser.cpp
    XWBParser.cpp
    XSBParser.cpp
)

add_library(asset_parsers STATIC ${PARSER_SOURCES})
target_include_directories(asset_parsers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(asset_parsers PUBLIC cxx_std_17)

if(MSVC)
    target_compile_definitions(asset_parsers PRIVATE strcasecmp=_stricmp strtok_r=strtok_s _CRT_SECURE_NO_WARNINGS)
endif()

if(glm_FOUND)
    target_link_libraries(asset_parsers PUBLIC glm::glm)
else()
    find_path(GLM_INCLUDE_DIR glm/glm.hpp)
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "GLM is required by asset_parsers. Install GLM or set CMAKE_PREFIX_PATH / glm_DIR.")
    endif()
    target_include_directories(asset_parsers PUBLIC ${GLM_INCLUDE_DIR})
endif()
