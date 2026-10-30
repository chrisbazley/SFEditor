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
add_executable(SFEditorLoadTest LoadGameData.c HostSupport.c ${loader_files})
target_link_libraries(SFEditorLoadTest PRIVATE CB CBDebug CBPseudo SF3K)
if(NOT MSVC)
    target_link_libraries(SFEditorLoadTest PRIVATE m)
endif()

option(SFEDITOR_TEST_COVERAGE "Instrument loading tests for GCC coverage reports" OFF)
if(SFEDITOR_TEST_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "SFEDITOR_TEST_COVERAGE requires GCC")
    endif()
    target_compile_options(SFEditorLoadTest PRIVATE --coverage)
    set_property(TARGET SFEditorLoadTest APPEND_STRING PROPERTY LINK_FLAGS " --coverage")
endif()

set(SFEDITOR_GAME_ARCHIVE "" CACHE FILEPATH "Existing sf3000.zip for offline loading tests")
foreach(group shared missions)
    add_test(NAME SFEditorLoad-${group}
        COMMAND "${CMAKE_COMMAND}"
            "-DLOADER=$<TARGET_FILE:SFEditorLoadTest>"
            "-DARCHIVE=${SFEDITOR_GAME_ARCHIVE}"
            "-DGROUP=${group}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/LoadGameData.cmake")
    set_tests_properties(SFEditorLoad-${group} PROPERTIES
        RESOURCE_LOCK SFEditorGameData TIMEOUT 120 LABELS "game-data")
endforeach()
