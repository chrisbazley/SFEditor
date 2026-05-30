cmake_minimum_required(VERSION 3.11)
project(SFEditor C)

include(FetchContent)

FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)
FetchContent_Declare(CB GIT_REPOSITORY https://github.com/chrisbazley/CBLibrary.git GIT_TAG main)
FetchContent_Declare(CBDebug GIT_REPOSITORY https://github.com/chrisbazley/CBDebugLib.git GIT_TAG main)
FetchContent_Declare(OptionalAcornC GIT_REPOSITORY https://github.com/chrisbazley/OptionalAcornC.git GIT_TAG main)
FetchContent_Declare(GKey GIT_REPOSITORY https://github.com/chrisbazley/GKeyLib.git GIT_TAG main)
FetchContent_Declare(CBOS GIT_REPOSITORY https://github.com/chrisbazley/CBOSLib.git GIT_TAG main)
FetchContent_Declare(SF3K GIT_REPOSITORY https://github.com/chrisbazley/SF3KLib.git GIT_TAG main)

FetchContent_MakeAvailable(CBUtil CB CBOS GKey CBDebug SF3K OptionalAcornC)

set(CMAKE_C_STANDARD 99)

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-compare -pedantic)
endif()

file(GLOB HEADER_FILES CONFIGURE_DEPENDS "*.h")

set(SOURCES
    PolyCol.c
    HillCol.c
    MapCoord.c
    Vertex.c
    Config.c
    IbarMenu.c
    AppIcon.c
    MapsMenu.c
    Session.c
    Main.c
    PreQuit.c
    Utils.c
    FilePaths.c
    FileScan.c
    BriefDbox.c
    GraphicsData.c
    MapMode.c
    ObjectsMode.c
    ShipsMode.c
    StatusBar.c
    DCS_dialogue.c
    MapToolbar.c
    ConfigBrush.c
    ConfigFill.c
    ConfigWand.c
    NewTransfer.c
    DrawTiles.c
    TransMenu.c
    Palette.c
    RenameTrans.c
    GraphicsFiles.c
    FilesMenus.c
    PathUtils.c
    MissOpts.c
    MissType.c
    FailThresh.c
    GroundLaser.c
    EditMenu.c
    MissFiles.c
    MapFiles.c
    SFFileInfo.c
    SFSaveAs.c
    Revert.c
    MainMenu.c
    ZoomMenu.c
    ToolMenu.c
    ModeMenu.c
    EffectMenu.c
    MPropDbox.c
    Plot.c
    ShipsMenu.c
    FPerfDbox.c
    BPerfDbox.c
    UtilsMenu.c
    ORDMenu.c
    SaveMap.c
    SaveMiss.c
    RenameMiss.c
    RenameMap.c
    EMHMenu.c
    RenMissMenu.c
    PlotMenu.c
    TransMenu2.c
    TransInfo.c
    SnakesMenu.c
    TilesMenu.c
    ObjGfxMesh.c
    MapTex.c
    SpecialShip.c
    ObjectsEdit.c
    MTransfersPalette.c
    TilesPalette.c
    MSnakesPalette.c
    Picker.c
    MapTexBitm.c
    GridCol.c
    BackCol.c
    MSnakes.c
    MapAnims.c
    MapEdit.c
    MTransfers.c
    Smooth.c
    EditWin.c
    MapEditSel.c
    MapEditChg.c
    GfxConfig.c
    SprMem.c
    Desktop.c
    Shapes.c
    Mission.c
    CoarseCoord.c
    Briefing.c
    Text.c
    Filenames.c
    Clouds.c
    Ships.c
    Paths.c
    Triggers.c
    Infos.c
    Defenc.c
    Player.c
    FPerf.c
    BPerf.c
    Pyram.c
    PerfMenu.c
    DFile.c
    Map.c
    Obj.c
    ParseArgs.c
    SFInit.c
    FSMenu.c
    ObjVertex.c
    ObjPolygon.c
    Editor.c
    ObjEditChg.c
    ObjsPalette.c
    ObjEditSel.c
    LayersMenu.c
    Snakes.c
    OSnakes.c
    OSnakesPalette.c
    ObjGfx.c
    DFileUtils.c
    SelCol.c
    DrawCloud.c
    OTransfers.c
    DrawObjs.c
    OPropDbox.c
    ConfigDbox.c
    GhostCol.c
    DrawTrig.c
    Hill.c
    OrientMenu.c
    ObjLayout.c
    MapLayout.c
    InfoMode.c
    SelBitmask.c
    IPropDbox.c
    InfoEditChg.c
    DrawInfo.c
    DrawInfos.c
    ITransfers.c
    InfoEdit.c
    MapAreaCol.c
    IPalette.c
    Goto.c
)

add_library(SFEditor ${SOURCES} ${HEADER_FILES})

target_link_libraries(SFEditor PUBLIC
    CB
    CBDebug
    OptionalAcornC
    SF3K
)

target_compile_definitions(SFEditor PUBLIC
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)

string(TOUPPER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME_UPPER)

if(SYSTEM_NAME_UPPER STREQUAL "RISCOS")
  # RISC OS: The only platform where it links and runs
  add_executable(SFEditorApp Main.c)
else()
  # Other platforms: Compile only (no linking) to catch errors
  add_library(SFEditorApp Main.c)
endif()

target_link_libraries(SFEditorApp PRIVATE
    SFEditor
)

include(CTest)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_subdirectory(tests)
endif()
