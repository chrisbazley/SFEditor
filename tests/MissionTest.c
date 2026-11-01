/* SFEditor test: mission composition.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "Writer.h"
#include "WriterMem.h"
#include "../Mission.h"
#include "../DFile.h"
#include "../CoarseCoord.h"
#include "../DataType.h"
#include "../Ships.h"
#include "../Paths.h"
#include "../Triggers.h"
#include "../Infos.h"
#include "../Briefing.h"
#include "../Player.h"
#include "../Defenc.h"
#include "../FPerf.h"
#include "../BPerf.h"
#include "../Clouds.h"
#include "../Pyram.h"
#include "../Filenames.h"

/* Whole-mission buffers are static rather than large stack allocations. */
enum { Capacity = 8192 };

typedef enum
{
  MatchAll,
  IgnoreType,
  IgnoreDock,
  IgnoreScanners,
  IgnoreImpervious,
  IgnoreTime,
} MatchField;

typedef struct
{
  MissionType type;
  int time_limit;
  bool dock, scanners, impervious;
} MissionValues;

static MissionValues pattern(unsigned phase)
{
  static MissionType const types[] = {
    MissionType_Normal, MissionType_Space, MissionType_Cyber,
  };
  static int const times[] = {1, 137, INT_MAX};
  return (MissionValues){
    .type = types[phase % ARRAY_SIZE(types)],
    .time_limit = times[phase % ARRAY_SIZE(times)],
    .dock = (phase & 1) != 0,
    .scanners = (phase & 2) != 0,
    .impervious = (phase & 4) != 0,
  };
}

static MissionData *create(void)
{
  _Optional MissionData *const mission = mission_create();
  assert(mission);
  return &*mission;
}

static void destroy(MissionData *mission)
{
  dfile_release(mission_get_dfile(mission));
}

static void check_fields(MissionData const *mission, unsigned phase, MatchField ignore)
{
  MissionValues const expected = pattern(phase);
  if (ignore != IgnoreType)
  {
    assert(mission_get_type(mission) == expected.type);
  }

  if (ignore != IgnoreDock)
  {
    assert(mission_get_dock_to_finish(mission) == expected.dock);
  }

  if (ignore != IgnoreScanners)
  {
    assert(mission_get_scanners_down(mission) == expected.scanners);
  }

  if (ignore != IgnoreImpervious)
  {
    assert(mission_get_impervious_map(mission) == expected.impervious);
  }

  if (ignore != IgnoreTime)
  {
    assert(mission_get_time_limit(mission) == expected.time_limit);
    assert(!mission_time_limit_is_disabled(mission));
  }
}

static MissionData *make_mission(unsigned phase)
{
  MissionData *const mission = create();
  MissionValues const values = pattern(phase);
  mission_set_type(mission, values.type);
  mission_set_dock_to_finish(mission, values.dock);
  mission_set_scanners_down(mission, values.scanners);
  mission_set_impervious_map(mission, values.impervious);
  mission_set_time_limit(mission, values.time_limit);

  PlayerData *const player = mission_get_player(mission);
  player_set_ship_type(player, ShipType_Fighter3);
  player_set_equip_enabled(player, true);
  player_set_shields(player, 7);
  player_set_engine(player, 9);
  player_set_control(player, 11);
  player_set_laser_type(player, 5);
  player_set_ata(player, 17);

  DefencesData *const defences = mission_get_defences(mission);
  defences_set_ship_type(defences, ShipType_Fighter2);
  defences_set_timer(defences, 321);
  defences_set_fire_prob(defences, 17);
  defences_set_laser_type(defences, 4);
  defences_set_ships_per_hangar(defences, 3);
  defences_set_ship_prob(defences, 19);
  pyramid_set_position(mission_get_pyramid(mission), Pyramid_Medium, 3);
  clouds_set_colour(mission_get_cloud_colours(mission), 0, 19);
  clouds_set_colour(mission_get_cloud_colours(mission), 1, 37);
  filenames_set(mission_get_filenames(mission), DataType_BaseMap, "Map");
  filenames_set(mission_get_filenames(mission), DataType_MapTextures, "Tiles");

  static ShipType const fighters[] = {
    ShipType_Fighter1, ShipType_Fighter2, ShipType_Fighter3, ShipType_Fighter4,
  };
  for (size_t i = 0; i < ARRAY_SIZE(fighters); ++i)
  {
    int const offset = fighters[i] - ShipType_Fighter1;
    FighterPerform *const fighter = fighter_perform_get_ship(
      mission_get_fighter_perform(mission), fighters[i]);
    fighter_perform_set_shields(fighter, FighterPerformMinShields + (100 * offset));
    fighter_perform_set_engine(fighter, FighterPerformMinEngine + (20 * offset));
    fighter_perform_set_control(fighter, FighterPerformMinControl + offset);
  }

  BigPerform *const big = big_perform_get_ship(mission_get_big_perform(mission), ShipType_Big2);
  big_perform_set_shields(big, 777);
  big_perform_set_ship_count(big, 4);
  big_perform_set_laser_type(big, 6);

  assert(!SFError_fail(briefing_add_text(mission_get_briefing(mission), "TITLE: MISSION TEST")));
  assert(!SFError_fail(briefing_add_text(mission_get_briefing(mission), "LOCATION: SOMEWHERE")));
  assert(!SFError_fail(briefing_add_text(mission_get_briefing(mission), "PERFORM THE MISSION.")));
  TargetInfosData *const infos = mission_get_target_infos(mission);
  assert(!SFError_fail(target_infos_add(infos, (MapPoint){7, 9}, NULL)));
  _Optional TargetInfo *const info = target_info_from_index(infos, 0);
  assert(info);
  assert(!SFError_fail(target_info_set_text(&*info, TargetInfoTextIndex_Type, "TARGET")));
  assert(!SFError_fail(target_info_set_text(&*info, TargetInfoTextIndex_Details, "DETAILS")));
  assert(!SFError_fail(triggers_add(mission_get_triggers(mission), (MapPoint){13, 17},
    (TriggerFullParam){.param = {.action = TriggerAction_MissionTarget, .value = 42},
                       .next_coords = {0, 0}})));

  _Optional Path *const path = paths_add(mission_get_paths(mission));
  assert(path);
  assert(path_add_waypoint(&*path, (CoarsePoint3d){3, 5, 7}));
  _Optional Waypoint *const waypoint = path_add_waypoint(&*path, (CoarsePoint3d){11, 13, 17});
  assert(waypoint);
  ShipsData *const ships = mission_get_ships(mission);
  _Optional Ship *first = NULL, *second = NULL, *third = NULL;
  ShipFlags const flags = {.is_friendly = true};
  assert(!SFError_fail(ships_add(ships, (FinePoint3d){100, 200, 300},
    ShipDirection_N, ShipType_Fighter1, ShipBehaviour_Moving, ShipMission_Target,
    flags, ShipPilot_CallumBlaze, &first)));
  assert(!SFError_fail(ships_add(ships, (FinePoint3d){101, 201, 301},
    ShipDirection_N, ShipType_Fighter1, ShipBehaviour_Moving, ShipMission_Target,
    flags, ShipPilot_CallumBlaze, &second)));
  assert(!SFError_fail(ships_add(ships, (FinePoint3d){102, 202, 302},
    ShipDirection_N, ShipType_Fighter1, ShipBehaviour_Moving, ShipMission_Target,
    flags, ShipPilot_CallumBlaze, &third)));
  assert(first);
  assert(second);
  assert(third);
  ship_set_attack(&*first, &*second);
  ship_set_follow(&*second, &*first, (FinePoint3d){101, 202, 303});
  ship_set_flightpath(&*third, &*waypoint);
  return mission;
}

static size_t write_data(MissionData *mission, unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  dfile_write(mission_get_dfile(mission), &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(MissionData *mission,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  Reader reader;
  assert(length <= sizeof(*buffer));
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = dfile_read(mission_get_dfile(mission), &reader);
  reader_destroy(&reader);
  return err;
}

static void check_components(MissionData *mission)
{
  PyramidData *const pyramid = mission_get_pyramid(mission);
  assert(pyramid_get_difficulty(pyramid) == Pyramid_Medium);
  assert(pyramid_get_level_number(pyramid) == 3);
  PlayerData *const player = mission_get_player(mission);
  assert(player_get_ship_type(player) == ShipType_Fighter3);
  assert(player_get_equip_enabled(player));
  assert(player_get_shields(player) == 7);
  assert(player_get_engine(player) == 9);
  assert(player_get_control(player) == 11);
  assert(player_get_laser_type(player) == 5);
  assert(player_get_ata(player) == 17);
  DefencesData *const defences = mission_get_defences(mission);
  assert(defences_get_ship_type(defences) == ShipType_Fighter2);
  assert(defences_get_timer(defences) == 321);
  assert(defences_get_fire_prob(defences) == 17);
  assert(defences_get_laser_type(defences) == 4);
  assert(defences_get_ships_per_hangar(defences) == 3);
  assert(defences_get_ship_prob(defences) == 19);
  assert(clouds_get_colour(mission_get_cloud_colours(mission), 0) == 19);
  assert(clouds_get_colour(mission_get_cloud_colours(mission), 1) == 37);
  assert(strcmp(filenames_get(mission_get_filenames(mission), DataType_BaseMap), "Map") == 0);
  assert(strcmp(filenames_get(mission_get_filenames(mission), DataType_MapTextures), "Tiles") == 0);

  static ShipType const fighters[] = {
    ShipType_Fighter1, ShipType_Fighter2, ShipType_Fighter3, ShipType_Fighter4,
  };
  for (size_t i = 0; i < ARRAY_SIZE(fighters); ++i)
  {
    int const offset = fighters[i] - ShipType_Fighter1;
    FighterPerform const *const fighter = fighter_perform_get_ship(
      mission_get_fighter_perform(mission), fighters[i]);
    assert(fighter_perform_get_shields(fighter) == (FighterPerformMinShields + (100 * offset)));
    assert(fighter_perform_get_engine(fighter) == (FighterPerformMinEngine + (20 * offset)));
    assert(fighter_perform_get_control(fighter) == (FighterPerformMinControl + offset));
  }

  BigPerform const *const big = big_perform_get_ship(mission_get_big_perform(mission), ShipType_Big2);
  assert(big_perform_get_shields(big) == 777);
  assert(big_perform_get_ship_count(big) == 4);
  assert(big_perform_get_laser_type(big) == 6);

  BriefingData *const briefing = mission_get_briefing(mission);
  assert(briefing_get_text_count(briefing) == 3);
  assert(strcmp(briefing_get_title(briefing), "MISSION TEST") == 0);
  assert(strcmp(briefing_get_text(briefing, 2), "PERFORM THE MISSION.") == 0);
  TargetInfosData *const infos = mission_get_target_infos(mission);
  assert(target_infos_get_count(infos) == 1);
  _Optional TargetInfo *const info = target_info_from_index(infos, 0);
  assert(info);
  MapPoint const info_pos = target_info_get_pos(&*info);
  assert(info_pos.x == 7);
  assert(info_pos.y == 9);
  assert(strcmp(target_info_get_text(&*info, TargetInfoTextIndex_Type), "TARGET") == 0);
  assert(strcmp(target_info_get_text(&*info, TargetInfoTextIndex_Details), "DETAILS") == 0);

  TriggersData *const triggers = mission_get_triggers(mission);
  assert(triggers_get_count(triggers) == 1);
  MapArea const area = {.min = {13, 17}, .max = {13, 17}};
  TriggersIter iter;
  TriggerFullParam param;
  MapPoint const trigger_pos = TriggersIter_get_first(&iter, triggers, &area, &param);
  assert(!TriggersIter_done(&iter));
  assert(trigger_pos.x == 13);
  assert(trigger_pos.y == 17);
  assert(param.param.action == TriggerAction_MissionTarget);
  assert(param.param.value == 42);
  TriggersIter_get_next(&iter, &param);
  assert(TriggersIter_done(&iter));
}

static void check_relationships(MissionData *mission)
{
  ShipsData *const ships = mission_get_ships(mission);
  assert(ships_get_count(ships) == 3);
  _Optional Ship *const first = ship_from_index(ships, 0);
  _Optional Ship *const second = ship_from_index(ships, 1);
  _Optional Ship *const third = ship_from_index(ships, 2);
  assert(first);
  assert(second);
  assert(third);
  assert(ship_get_mode(&*first) == ShipMode_Attack);
  assert(ship_get_attack(&*first) == second);
  assert(ship_get_mode(&*second) == ShipMode_FollowShip);
  assert(ship_get_follow(&*second) == first);
  FinePoint3d const offset = ship_get_follow_offset(&*second);
  assert(offset.x == 101);
  assert(offset.y == 202);
  assert(offset.z == 303);

  _Optional Path *const path = path_from_index(mission_get_paths(mission), 0);
  assert(path);
  assert(path_from_index(mission_get_paths(mission), 1) == NULL);
  _Optional Waypoint *const waypoint = waypoint_from_index(&*path, 1);
  assert(waypoint);
  CoarsePoint3d const coords = waypoint_get_coords(&*waypoint);
  assert(coords.x == 11);
  assert(coords.y == 13);
  assert(coords.z == 17);
  assert(ship_get_mode(&*third) == ShipMode_FlightPath);
  assert(ship_get_flightpath(&*third) == waypoint);
  assert(waypoint_get_path(ship_get_flightpath(&*third)) == path);
}

/* Locate changed bytes by comparing two valid writer outputs, not by
 * assuming where the selected field is stored. */
static void check_invalid_field(MatchField field, SFErrorType expected)
{
  MissionData *const source = make_mission(0);
  static unsigned char before[Capacity], after[Capacity];
  size_t const before_size = write_data(source, &before);

  switch (field)
  {
    case IgnoreType:
      mission_set_type(source, MissionType_Cyber);
      break;

    case IgnoreDock:
      mission_set_dock_to_finish(source, true);
      break;

    case IgnoreScanners:
      mission_set_scanners_down(source, true);
      break;

    case IgnoreImpervious:
      mission_set_impervious_map(source, true);
      break;

    case IgnoreTime:
      mission_set_time_limit(source, INT_MAX);
      break;

    default:
      assert(false);
      break;
  }

  size_t const after_size = write_data(source, &after);
  size_t const limit = LOWEST(before_size, after_size);
  MissionData *const loaded = create();
  bool found = false;

  for (size_t i = 0; i < limit; ++i)
  {
    if (before[i] == after[i])
    {
      continue;
    }

    unsigned char const original = before[i];
    before[i] = UCHAR_MAX;
    SFError const err = read_data(loaded, &before, before_size);
    before[i] = original;

    if (err.type == expected)
    {
      found = true;
      break;
    }
  }

  assert(found);

  destroy(loaded);
  destroy(source);
}

static void test1(void)
{
  /* Initial mission settings */

  MissionData *const mission = create();

  assert(mission_get_type(mission) == MissionType_Normal);
  assert(!mission_get_dock_to_finish(mission));
  assert(!mission_get_scanners_down(mission));
  assert(!mission_get_impervious_map(mission));
  assert(mission_time_limit_is_disabled(mission));
  assert(mission_get_time_limit(mission) == 0);

  destroy(mission);
}

static void test2(void)
{
  /* Initial mission collections */

  MissionData *const mission = create();

  assert(triggers_get_count(mission_get_triggers(mission)) == 0);
  assert(target_infos_get_count(mission_get_target_infos(mission)) == 0);
  assert(ships_get_count(mission_get_ships(mission)) == 0);
  assert(briefing_get_text_count(mission_get_briefing(mission)) == 0);
  assert(path_from_index(mission_get_paths(mission), 0) == NULL);

  destroy(mission);
}

static void test3(void)
{
  /* Mission type setter */

  MissionData *const mission = make_mission(7);
  static MissionType const types[] = {MissionType_Normal, MissionType_Space, MissionType_Cyber};

  for (size_t i = 0; i < ARRAY_SIZE(types); ++i)
  {
    mission_set_type(mission, types[i]);

    assert(mission_get_type(mission) == types[i]);
    check_fields(mission, 7, IgnoreType);
  }

  destroy(mission);
}

static void test4(void)
{
  /* Dock-to-finish setter */

  MissionData *const mission = make_mission(7);
  static bool const values[] = {false, true};

  for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
  {
    mission_set_dock_to_finish(mission, values[i]);

    assert(mission_get_dock_to_finish(mission) == values[i]);
    check_fields(mission, 7, IgnoreDock);
  }

  destroy(mission);
}

static void test5(void)
{
  /* Scanners-down setter */

  MissionData *const mission = make_mission(7);
  static bool const values[] = {false, true};

  for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
  {
    mission_set_scanners_down(mission, values[i]);

    assert(mission_get_scanners_down(mission) == values[i]);
    check_fields(mission, 7, IgnoreScanners);
  }

  destroy(mission);
}

static void test6(void)
{
  /* Impervious-map setter */

  MissionData *const mission = make_mission(7);
  static bool const values[] = {false, true};

  for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
  {
    mission_set_impervious_map(mission, values[i]);

    assert(mission_get_impervious_map(mission) == values[i]);
    check_fields(mission, 7, IgnoreImpervious);
  }

  destroy(mission);
}

static void test7(void)
{
  /* Mission time limit setter */

  MissionData *const mission = make_mission(7);
  static int const times[] = {1, 137, INT_MAX};
  mission_disable_time_limit(mission);

  for (size_t i = 0; i < ARRAY_SIZE(times); ++i)
  {
    mission_set_time_limit(mission, times[i]);

    assert(mission_get_time_limit(mission) == times[i]);
    assert(!mission_time_limit_is_disabled(mission));
    check_fields(mission, 7, IgnoreTime);
  }

  destroy(mission);
}

static void test8(void)
{
  /* Mission time limit disable */

  MissionData *const mission = make_mission(7);

  mission_disable_time_limit(mission);

  assert(mission_time_limit_is_disabled(mission));
  assert(mission_get_time_limit(mission) == 0);
  check_fields(mission, 7, IgnoreTime);

  destroy(mission);
}

static void test9(void)
{
  /* Independent mission settings */

  MissionData *const first = make_mission(0);
  MissionData *const second = make_mission(7);

  mission_set_time_limit(first, 42);

  check_fields(second, 7, MatchAll);

  destroy(second);
  destroy(first);
}

static void test10(void)
{
  /* Mission settings round trip */

  for (unsigned phase = 0; phase < 8; ++phase)
  {
    MissionData *const source = make_mission(phase);
    static unsigned char buffer[Capacity];
    size_t const length = write_data(source, &buffer);
    MissionData *const loaded = create();

    assert(!SFError_fail(read_data(loaded, &buffer, length)));

    check_fields(loaded, phase, MatchAll);

    destroy(loaded);
    destroy(source);
  }
}

static void test11(void)
{
  /* Mission components round trip */

  MissionData *const source = make_mission(0);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = create();

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  check_components(loaded);

  destroy(loaded);
  destroy(source);
}

static void test12(void)
{
  /* Mission ship relationships round trip */

  MissionData *const source = make_mission(0);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = create();

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  check_relationships(loaded);

  destroy(loaded);
  destroy(source);
}

static void test13(void)
{
  /* Disabled mission timer round trip */

  MissionData *const source = make_mission(0);
  mission_disable_time_limit(source);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = create();

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  assert(mission_time_limit_is_disabled(loaded));
  assert(mission_get_time_limit(loaded) == 0);
  check_fields(loaded, 0, IgnoreTime);

  destroy(loaded);
  destroy(source);
}

static void test14(void)
{
  /* Mission reload replaces collections */

  MissionData *const source = make_mission(0);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = make_mission(7);

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  check_fields(loaded, 0, MatchAll);
  check_components(loaded);
  check_relationships(loaded);

  destroy(loaded);
  destroy(source);
}

static void test15(void)
{
  /* Invalid mission type */

  check_invalid_field(IgnoreType, SFErrorType_BadMissionType);
}

static void test16(void)
{
  /* Invalid dock-to-finish value */

  check_invalid_field(IgnoreDock, SFErrorType_BadEndDocked);
}

static void test17(void)
{
  /* Invalid scanner flag */

  check_invalid_field(IgnoreScanners, SFErrorType_BadScannersDown);
}

static void test18(void)
{
  /* Invalid impervious-map flag */

  check_invalid_field(IgnoreImpervious, SFErrorType_BadImperviousMap);
}

static void test19(void)
{
  /* Negative mission timer */

  check_invalid_field(IgnoreTime, SFErrorType_BadMissionTimer);
}

static void test20(void)
{
  /* Empty mission input */

  MissionData *const mission = make_mission(0);
  static unsigned char buffer[Capacity];

  assert(SFError_fail(read_data(mission, &buffer, 0)));

  destroy(mission);
}

static void test21(void)
{
  /* Mission output failure */

  MissionData *const mission = make_mission(0);
  unsigned char buffer[64];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  dfile_write(mission_get_dfile(mission), &writer);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  destroy(mission);
}

#ifdef FORTIFY
static void test22(void)
{
  /* Mission creation allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    _Optional MissionData *const mission = mission_create();
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (mission)
    {
      assert(limit > 0);
      destroy(&*mission);
      succeeded = true;
      break;
    }
  }

  assert(succeeded);
}

static void test23(void)
{
  /* Mission failed reload destruction */

  MissionData *const source = make_mission(0);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = make_mission(7);
  Reader reader;
  assert(reader_mem_init(&reader, buffer, length));

  Fortify_SetNumAllocationsLimit(0);
  SFError const err = dfile_read(mission_get_dfile(loaded), &reader);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);

  assert(err.type == SFErrorType_NoMem);

  reader_destroy(&reader);
  destroy(loaded);
  destroy(source);
}

static void test24(void)
{
  /* Mission reload retry after allocation failure */

  MissionData *const source = make_mission(0);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  MissionData *const loaded = make_mission(7);
  Reader reader;
  assert(reader_mem_init(&reader, buffer, length));

  Fortify_SetNumAllocationsLimit(0);
  SFError const err = dfile_read(mission_get_dfile(loaded), &reader);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(err.type == SFErrorType_NoMem);
  reader_destroy(&reader);

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  check_fields(loaded, 0, MatchAll);
  check_components(loaded);
  check_relationships(loaded);

  destroy(loaded);
  destroy(source);
}
#endif

void Mission_tests(void)
{
  /* Mission */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial mission settings", test1 },
    { "Initial mission collections", test2 },
    { "Mission type setter", test3 },
    { "Dock-to-finish setter", test4 },
    { "Scanners-down setter", test5 },
    { "Impervious-map setter", test6 },
    { "Mission time limit setter", test7 },
    { "Mission time limit disable", test8 },
    { "Independent mission settings", test9 },
    { "Mission settings round trip", test10 },
    { "Mission components round trip", test11 },
    { "Mission ship relationships round trip", test12 },
    { "Disabled mission timer round trip", test13 },
    { "Mission reload replaces collections", test14 },
    { "Invalid mission type", test15 },
    { "Invalid dock-to-finish value", test16 },
    { "Invalid scanner flag", test17 },
    { "Invalid impervious-map flag", test18 },
    { "Negative mission timer", test19 },
    { "Empty mission input", test20 },
    { "Mission output failure", test21 },
#ifdef FORTIFY
    { "Mission creation allocation failure", test22 },
    { "Mission failed reload destruction", test23 },
    { "Mission reload retry after allocation failure", test24 },
#endif
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n", 1 + count,
           ARRAY_SIZE(unit_tests), unit_tests[count].test_name);
    fflush(stdout);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
