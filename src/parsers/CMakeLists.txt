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
    NSDEntityResolver.cpp
    NSDEntityType.cpp
    XPRParser.cpp
    NLTParser.cpp
    TGAParser.cpp
    NOCParser.cpp
    NLSParser.cpp
    NOTParser.cpp
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

if(MSVC)
    target_compile_definitions(asset_parsers PRIVATE strcasecmp=_stricmp strtok_r=strtok_s)
endif()

if(glm_FOUND)
    target_link_libraries(asset_parsers PUBLIC glm::glm)
else()
    message(STATUS "GLM not found via find_package, assuming system headers available")
endif()
