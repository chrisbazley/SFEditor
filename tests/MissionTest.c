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

static void test1(void)
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

#ifdef FORTIFY
static void test2(void)
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

#endif

#ifdef FORTIFY
static void test3(void)
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
  assert(ships_get_count(mission_get_ships(loaded)) == 3);
  assert(strcmp(briefing_get_title(mission_get_briefing(loaded)), "MISSION TEST") == 0);

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
    { "Mission settings round trip", test1 },
#ifdef FORTIFY
    { "Mission failed reload destruction", test2 },
#endif
#ifdef FORTIFY
    { "Mission reload retry after allocation failure", test3 },
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
