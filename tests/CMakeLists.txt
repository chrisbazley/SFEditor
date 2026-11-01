if(NOT BUILD_TESTING OR SYSTEM_NAME_UPPER STREQUAL "RISCOS")
    return()
endif()

# Compile the production file models, without the application or RISC OS
# sprite implementation. Loaders have no dependencies on UI/rendering code.
set(loader_sources
    DFile Map Obj MapCoord PolyCol HillCol MapTexBitm ObjGfxRead ObjVertex
    ObjPolygon Mission Player Triggers Infos Ships Paths Briefing Text
    Filenames Clouds Defenc FPerf BPerf Pyram CoarseCoord DataUtils)
set(loader_files)
foreach(source IN LISTS loader_sources)
    list(APPEND loader_files "${PROJECT_SOURCE_DIR}/${source}.c")
endforeach()
# One invocation runs every test group after CMake prepares the game data.
add_executable(SFEditorTests
    Main.c LoadGameData.c HostSupport.c
    FPerfTest.c BPerfTest.c
    CoarseCoordTest.c FilenamesTest.c PyramTest.c CloudsTest.c
    PlayerTest.c DefencTest.c
    DataUtilsTest.c
    PathsTest.c BriefingTest.c TextTest.c MapTest.c ObjTest.c
    VertexTest.c ShapesTest.c MapCoordTest.c MapAreaColTest.c SelBitmaskTest.c
    MapEditSelTest.c ObjEditSelTest.c
    ${loader_files}
    "${PROJECT_SOURCE_DIR}/MapAreaCol.c"
    "${PROJECT_SOURCE_DIR}/SelBitmask.c"
    "${PROJECT_SOURCE_DIR}/MapEditSel.c"
    "${PROJECT_SOURCE_DIR}/ObjEditSel.c"
    "${PROJECT_SOURCE_DIR}/Shapes.c"
    "${PROJECT_SOURCE_DIR}/Vertex.c")
target_link_libraries(SFEditorTests PRIVATE CB CBDebug CBPseudo SF3K)

# SFError's layout depends on NDEBUG, so models and tests must agree.
if(MSVC)
    target_compile_options(SFEditorTests PRIVATE /UNDEBUG)
else()
    target_compile_options(SFEditorTests PRIVATE -UNDEBUG)
    target_link_libraries(SFEditorTests PRIVATE m)
endif()

# Show Debug suite names without enabling verbose production diagnostics.
set_source_files_properties(
    Main.c LoadGameData.c
    FPerfTest.c BPerfTest.c
    CoarseCoordTest.c FilenamesTest.c PyramTest.c CloudsTest.c
    PlayerTest.c DefencTest.c
    DataUtilsTest.c
    PathsTest.c BriefingTest.c TextTest.c MapTest.c ObjTest.c
    VertexTest.c ShapesTest.c MapCoordTest.c MapAreaColTest.c
    SelBitmaskTest.c MapEditSelTest.c ObjEditSelTest.c
    PROPERTIES
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:DEBUG_OUTPUT>")

option(SFEDITOR_TEST_COVERAGE "Instrument portable tests for GCC coverage reports" OFF)
if(SFEDITOR_TEST_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "SFEDITOR_TEST_COVERAGE requires GCC")
    endif()
    target_compile_options(SFEditorTests PRIVATE --coverage)
    set_property(TARGET SFEditorTests APPEND_STRING PROPERTY LINK_FLAGS " --coverage")
endif()

set(SFEDITOR_GAME_ARCHIVE "" CACHE FILEPATH "Existing sf3000.zip for offline tests")
add_test(NAME SFEditorTests
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_PROGRAM=$<TARGET_FILE:SFEditorTests>"
        "-DARCHIVE=${SFEDITOR_GAME_ARCHIVE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/LoadGameData.cmake")
set_tests_properties(SFEditorTests PROPERTIES
    TIMEOUT 180 LABELS "unit;selection;game-data")
