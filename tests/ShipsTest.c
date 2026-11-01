/* SFEditor test: mission ships.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
#include "../CoarseCoord.h"
#include "../Ships.h"
#include "../ShipsData.h"
#include "../Paths.h"
#include "../PathsData.h"

/* Full padded ship lists fit here; keep the buffer off the stack. */
enum { Capacity = 2048 };

typedef struct
{
  FinePoint3d coords;
  ShipDirection direction;
  ShipType type;
  ShipBehaviour behaviour;
  ShipMission importance;
  ShipFlags flags;
  ShipPilot pilot;
} ShipValues;

typedef enum
{
  MatchAll,
  IgnoreMode,
} MatchField;

static ShipValues pattern(unsigned index)
{
  static ShipType const types[] = {
    ShipType_Fighter1, ShipType_Fighter2, ShipType_Fighter3, ShipType_Fighter4,
    ShipType_Big1, ShipType_Big2, ShipType_Big3, ShipType_Satellite,
  };
  static ShipDirection const directions[] = {
    ShipDirection_S, ShipDirection_SE, ShipDirection_E, ShipDirection_NE,
    ShipDirection_N, ShipDirection_NW, ShipDirection_W, ShipDirection_SW,
  };
  static ShipBehaviour const behaviours[] = {
    ShipBehaviour_Moving, ShipBehaviour_TurningWheel,
    ShipBehaviour_SpinningTop, ShipBehaviour_Stationary,
  };
  static ShipMission const goals[] = {
    ShipMission_NotImportant, ShipMission_Target, ShipMission_Protect,
    ShipMission_ProtectUntilArrival, ShipMission_NoPlayerData,
    ShipMission_PreventLanding,
  };
  static ShipPilot const pilots[] = {
    ShipPilot_None, ShipPilot_CallumBlaze, ShipPilot_LukeForester,
    ShipPilot_JaneHollyDean, ShipPilot_HazelPhoenix, ShipPilot_DaveValiant,
    ShipPilot_StefanLancaster, ShipPilot_Crusher, ShipPilot_AlliedPilot,
    ShipPilot_LeslieJacobs, ShipPilot_JakePhillips, ShipPilot_Spyder,
    ShipPilot_JohnTBooker, ShipPilot_KellyForester,
    ShipPilot_TraineePilotI, ShipPilot_TraineePilotII,
  };

  return (ShipValues){
    .coords = {0x01020304u * (index + 1), 0xfedcba98u - index, index * 31337u},
    .direction = directions[index % ARRAY_SIZE(directions)],
    .type = types[index % ARRAY_SIZE(types)],
    .behaviour = behaviours[index % ARRAY_SIZE(behaviours)],
    .importance = goals[index % ARRAY_SIZE(goals)],
    .flags = {
      .is_friendly = (index & 1) != 0,
      .is_cloaked = (index & 2) != 0,
      .is_pacifist = (index & 4) != 0,
      .is_alert = (index & 8) != 0,
    },
    .pilot = pilots[index % ARRAY_SIZE(pilots)],
  };
}

static Ship *add_ship(ShipsData *ships, unsigned index)
{
  ShipValues const values = pattern(index);
  _Optional Ship *ship = NULL;
  assert(!SFError_fail(ships_add(ships, values.coords, values.direction,
    values.type, values.behaviour, values.importance, values.flags,
    values.pilot, &ship)));
  assert(ship);
  return &*ship;
}

static void populate(ShipsData *ships, unsigned count)
{
  ships_init(ships);

  for (unsigned i = 0; i < count; ++i)
  {
    add_ship(ships, i);
  }
}

static void check_point(FinePoint3d actual, FinePoint3d expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
  assert(actual.z == expected.z);
}

static void check_flags(ShipFlags actual, ShipFlags expected)
{
  assert(actual.is_friendly == expected.is_friendly);
  assert(actual.is_cloaked == expected.is_cloaked);
  assert(actual.is_pacifist == expected.is_pacifist);
  assert(actual.is_alert == expected.is_alert);
}

static void check_fields(Ship const *ship, unsigned index, MatchField ignore)
{
  ShipValues const expected = pattern(index);
  check_point(ship_get_coords(ship), expected.coords);
  assert(ship_get_direction(ship) == expected.direction);
  assert(ship_get_type(ship) == expected.type);
  assert(ship_get_behaviour(ship) == expected.behaviour);
  assert(ship_get_importance(ship) == expected.importance);
  check_flags(ship_get_flags(ship), expected.flags);
  assert(ship_get_pilot(ship) == expected.pilot);

  if (ignore != IgnoreMode)
  {
    assert(ship_get_mode(ship) == ShipMode_Attack);
  }
}

static void check_pattern(ShipsData *ships, unsigned count)
{
  assert(ships_get_count(ships) == count);

  for (unsigned i = 0; i < count; ++i)
  {
    _Optional Ship *const ship = ship_from_index(ships, i);
    assert(ship);
    check_fields(&*ship, i, MatchAll);
    assert(ship_get_attack(&*ship) == NULL);
  }

  assert(ship_from_index(ships, count) == NULL);
}

static size_t write_data(ShipsData *ships, bool padded,
  unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));

  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  ships_pre_write(ships);

  if (padded)
  {
    ships_write_pad(ships, &writer);
  }
  else
  {
    ships_write(ships, &writer);
  }

  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(ShipsData *ships, PathsData *paths, bool padded,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));

  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError err = padded ? ships_read_pad(ships, &reader) : ships_read(ships, &reader);

  if (!SFError_fail(err))
  {
    err = ships_post_read(ships, paths);
  }

  reader_destroy(&reader);
  return err;
}

static void copy_paths(PathsData *source, PathsData *loaded)
{
  static unsigned char buffer[4096];
  memset(buffer, 0, sizeof(buffer));
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));
  paths_pre_write(source);
  paths_write(source, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(buffer));
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);

  paths_init(loaded);
  Reader reader;
  assert(reader_mem_init(&reader, buffer, (size_t)length));
  assert(!SFError_fail(paths_read(loaded, &reader)));
  reader_destroy(&reader);
}

static void test1(void)
{
  /* Ship flags round trip */

  for (unsigned flags = 0; flags < 16; ++flags)
  {
    ShipsData source;
    populate(&source, 0);
    add_ship(&source, flags);

    static unsigned char buffer[Capacity];
    size_t const length = write_data(&source, false, &buffer);

    PathsData paths;
    paths_init(&paths);
    ShipsData loaded;
    ships_init(&loaded);
    assert(!SFError_fail(read_data(&loaded, &paths, false, &buffer, length)));

    _Optional Ship *const ship = ship_from_index(&loaded, 0);
    assert(ship);
    check_flags(ship_get_flags(&*ship), pattern(flags).flags);

    ships_destroy(&loaded);
    paths_destroy(&paths);
    ships_destroy(&source);
  }
}

static void test2(void)
{
  /* Initial ship count */

  ShipsData ships;
  ships_init(&ships);

  assert(ships_get_count(&ships) == 0);
  assert(ship_from_index(&ships, 0) == NULL);
  assert(ship_from_index(&ships, -1) == NULL);

  ships_destroy(&ships);
}

static void test3(void)
{
  /* Added ship fields */

  ShipsData ships;
  populate(&ships, ShipsMax);

  check_pattern(&ships, ShipsMax);

  ships_destroy(&ships);
}

static void test4(void)
{
  /* Ship capacity */

  ShipsData ships;
  populate(&ships, ShipsMax);
  ShipValues const values = pattern(0);
  _Optional Ship *extra = ship_from_index(&ships, 0);

  SFError const err = ships_add(&ships, values.coords, values.direction,
    values.type, values.behaviour, values.importance, values.flags, values.pilot, &extra);

  assert(err.type == SFErrorType_NumShips);
  assert(extra == NULL);
  check_pattern(&ships, ShipsMax);

  ships_destroy(&ships);
}

static void test5(void)
{
  /* Ship addition without result */

  ShipsData ships;
  ships_init(&ships);
  ShipValues const values = pattern(0);

  assert(!SFError_fail(ships_add(&ships, values.coords, values.direction,
    values.type, values.behaviour, values.importance, values.flags, values.pilot, NULL)));

  check_pattern(&ships, 1);

  ships_destroy(&ships);
}

static void test6(void)
{
  /* Ship deletion preserves survivors */

  ShipsData ships;
  populate(&ships, ShipsMax);

  for (unsigned i = 0; i < ShipsMax / 2; ++i)
  {
    _Optional Ship *const ship = ship_from_index(&ships, i);
    assert(ship);
    ship_delete(&*ship);
  }

  assert(ships_get_count(&ships) == ShipsMax / 2);

  for (unsigned i = 0; i < ShipsMax / 2; ++i)
  {
    _Optional Ship *const ship = ship_from_index(&ships, i);
    assert(ship);
    check_fields(&*ship, (2 * i) + 1, MatchAll);
  }

  ships_destroy(&ships);
}

static void test7(void)
{
  /* Ship indices after deletion */

  ShipsData ships;
  populate(&ships, 3);
  _Optional Ship *const removed = ship_from_index(&ships, 1);
  assert(removed);
  ship_delete(&*removed);

  ships_pre_write(&ships);

  for (int i = 0; i < 2; ++i)
  {
    _Optional Ship *const ship = ship_from_index(&ships, i);
    assert(ship);
    assert(ship_get_index(&*ship) == i);
  }

  ships_destroy(&ships);
}

static void test8(void)
{
  /* Ship attack setter */

  ShipsData ships;
  populate(&ships, 0);
  Ship *const ship = add_ship(&ships, 0);
  Ship *const target = add_ship(&ships, 1);
  ship_set_follow(ship, target, (FinePoint3d){1, 2, 3});

  ship_set_attack(ship, target);

  assert(ship_get_mode(ship) == ShipMode_Attack);
  assert(ship_get_attack(ship) == target);
  check_fields(ship, 0, MatchAll);
  check_fields(target, 1, MatchAll);

  ships_destroy(&ships);
}

static void test9(void)
{
  /* Ship attack player */

  ShipsData ships;
  populate(&ships, 0);
  Ship *const ship = add_ship(&ships, 0);
  ship_set_attack(ship, add_ship(&ships, 1));

  ship_set_attack(ship, NULL);

  assert(ship_get_mode(ship) == ShipMode_Attack);
  assert(ship_get_attack(ship) == NULL);
  check_fields(ship, 0, MatchAll);

  ships_destroy(&ships);
}

static void test10(void)
{
  /* Ship follow setter */

  ShipsData ships;
  populate(&ships, 0);
  Ship *const ship = add_ship(&ships, 0);
  Ship *const leader = add_ship(&ships, 1);
  FinePoint3d const offset = {UINT32_MAX, 17, 0x12345678u};

  ship_set_follow(ship, leader, offset);

  assert(ship_get_mode(ship) == ShipMode_FollowShip);
  assert(ship_get_follow(ship) == leader);
  check_point(ship_get_follow_offset(ship), offset);
  check_fields(ship, 0, IgnoreMode);
  check_fields(leader, 1, MatchAll);

  ships_destroy(&ships);
}

static void test11(void)
{
  /* Ship follow player */

  ShipsData ships;
  populate(&ships, 0);
  Ship *const ship = add_ship(&ships, 0);
  FinePoint3d const offset = {3, 5, 7};

  ship_set_follow(ship, NULL, offset);

  assert(ship_get_mode(ship) == ShipMode_FollowShip);
  assert(ship_get_follow(ship) == NULL);
  check_point(ship_get_follow_offset(ship), offset);
  check_fields(ship, 0, IgnoreMode);

  ships_destroy(&ships);
}

static void test12(void)
{
  /* Ship flightpath setter */

  PathsData paths;
  paths_init(&paths);
  _Optional Path *const path = paths_add(&paths);
  assert(path);
  _Optional Waypoint *const waypoint = path_add_waypoint(&*path, (CoarsePoint3d){3, 5, 7});
  assert(waypoint);
  ShipsData ships;
  populate(&ships, 0);
  Ship *const ship = add_ship(&ships, 0);

  ship_set_flightpath(ship, &*waypoint);

  assert(ship_get_mode(ship) == ShipMode_FlightPath);
  assert(ship_get_flightpath(ship) == waypoint);
  check_fields(ship, 0, IgnoreMode);

  ships_destroy(&ships);
  paths_destroy(&paths);
}

static void test13(void)
{
  /* Ship round trip */

  PathsData paths;
  paths_init(&paths);

  for (int padded = 0; padded < 2; ++padded)
  {
    for (unsigned count = 0; count <= ShipsMax; ++count)
    {
      ShipsData source;
      populate(&source, count);
      static unsigned char buffer[Capacity];
      size_t const length = write_data(&source, padded != 0, &buffer);
      ShipsData loaded;
      ships_init(&loaded);

      assert(!SFError_fail(read_data(&loaded, &paths, padded != 0, &buffer, length)));

      check_pattern(&loaded, count);

      ships_destroy(&loaded);
      ships_destroy(&source);
    }
  }

  paths_destroy(&paths);
}

static void test14(void)
{
  /* Ship relationship round trip */

  PathsData paths;
  paths_init(&paths);
  _Optional Path *const path = paths_add(&paths);
  assert(path);
  assert(path_add_waypoint(&*path, (CoarsePoint3d){1, 2, 3}));
  _Optional Waypoint *const waypoint = path_add_waypoint(&*path, (CoarsePoint3d){5, 7, 9});
  assert(waypoint);
  PathsData loaded_paths;
  copy_paths(&paths, &loaded_paths);

  for (int padded = 0; padded < 2; ++padded)
  {
    ShipsData source;
    populate(&source, 0);
    Ship *const first = add_ship(&source, 0);
    Ship *const second = add_ship(&source, 1);
    Ship *const third = add_ship(&source, 2);
    ship_set_attack(first, second);
    ship_set_follow(second, first, (FinePoint3d){UINT32_MAX, 0, 123456789u});
    ship_set_flightpath(third, &*waypoint);

    static unsigned char buffer[Capacity];
    size_t const length = write_data(&source, padded != 0, &buffer);
    ShipsData loaded;
    ships_init(&loaded);
    assert(!SFError_fail(read_data(&loaded, &loaded_paths, padded != 0, &buffer, length)));

    _Optional Ship *const loaded_first = ship_from_index(&loaded, 0);
    _Optional Ship *const loaded_second = ship_from_index(&loaded, 1);
    _Optional Ship *const loaded_third = ship_from_index(&loaded, 2);
    assert(loaded_first);
    assert(loaded_second);
    assert(loaded_third);
    assert(ship_get_mode(&*loaded_first) == ShipMode_Attack);
    assert(ship_get_attack(&*loaded_first) == loaded_second);
    assert(ship_get_mode(&*loaded_second) == ShipMode_FollowShip);
    assert(ship_get_follow(&*loaded_second) == loaded_first);
    check_point(ship_get_follow_offset(&*loaded_second), (FinePoint3d){UINT32_MAX, 0, 123456789u});
    assert(ship_get_mode(&*loaded_third) == ShipMode_FlightPath);
    _Optional Path *const loaded_path = path_from_index(&loaded_paths, 0);
    assert(loaded_path);
    _Optional Waypoint *const loaded_waypoint = waypoint_from_index(&*loaded_path, 1);
    assert(loaded_waypoint);
    assert(ship_get_flightpath(&*loaded_third) == loaded_waypoint);

    for (unsigned i = 0; i < 3; ++i)
    {
      _Optional Ship *const ship = ship_from_index(&loaded, i);
      assert(ship);
      check_fields(&*ship, i, IgnoreMode);
    }

    ships_destroy(&loaded);
    ships_destroy(&source);
  }

  paths_destroy(&loaded_paths);
  paths_destroy(&paths);
}

static void test15(void)
{
  /* Ship follow player round trip */

  PathsData paths;
  paths_init(&paths);
  ShipsData source;
  populate(&source, 0);
  Ship *const ship = add_ship(&source, 0);
  ship_set_follow(ship, NULL, (FinePoint3d){7, UINT32_MAX, 11});

  static unsigned char buffer[Capacity];
  size_t const length = write_data(&source, false, &buffer);
  ShipsData loaded;
  ships_init(&loaded);
  assert(!SFError_fail(read_data(&loaded, &paths, false, &buffer, length)));

  _Optional Ship *const actual = ship_from_index(&loaded, 0);
  assert(actual);
  assert(ship_get_follow(&*actual) == NULL);
  check_point(ship_get_follow_offset(&*actual), (FinePoint3d){7, UINT32_MAX, 11});

  ships_destroy(&loaded);
  ships_destroy(&source);
  paths_destroy(&paths);
}

static void test16(void)
{
  /* Ship truncation */

  PathsData paths;
  paths_init(&paths);
  ShipsData source;
  populate(&source, 3);

  for (int padded = 0; padded < 2; ++padded)
  {
    static unsigned char buffer[Capacity];
    size_t const size = write_data(&source, padded != 0, &buffer);

    for (size_t length = 0; length < size; ++length)
    {
      ShipsData loaded;
      ships_init(&loaded);
      SFError const err = read_data(&loaded, &paths, padded != 0, &buffer, length);

      if (length == 0)
      {
        assert(SFError_fail(err));
      }

      if (!SFError_fail(err))
      {
        check_pattern(&loaded, 3);
      }

      ships_destroy(&loaded);
    }
  }

  ships_destroy(&source);
  paths_destroy(&paths);
}

static void test17(void)
{
  /* Corrupt ship data */

  PathsData paths;
  paths_init(&paths);
  _Optional Path *const path = paths_add(&paths);
  assert(path);
  _Optional Waypoint *const waypoint = path_add_waypoint(&*path, (CoarsePoint3d){1, 2, 3});
  assert(waypoint);
  PathsData loaded_paths;
  copy_paths(&paths, &loaded_paths);
  static SFErrorType const expected[] = {
    SFErrorType_BadNumShips, SFErrorType_BadShipType, SFErrorType_BadShipMode,
    SFErrorType_ReservedShipBits, SFErrorType_ReservedShipBits2,
    SFErrorType_BadShipGoal, SFErrorType_BadShipMotion, SFErrorType_BadShipDir,
    SFErrorType_BadShipPath, SFErrorType_BadShipWaypoint,
    SFErrorType_BadShipTarget, SFErrorType_BadShipLeader, SFErrorType_ReadFail,
  };
  bool seen[ARRAY_SIZE(expected)] = {false};
  size_t remaining = ARRAY_SIZE(expected);

  /* Chosen by comparing loader branch coverage with the exhaustive sweep. */
  static unsigned char const values[] = {0, 5, 32, UCHAR_MAX};

  static ShipMode const modes[] = {
    ShipMode_Attack, ShipMode_FollowShip, ShipMode_FlightPath,
  };

  for (size_t mode = 0; mode < ARRAY_SIZE(modes); ++mode)
  {
    ShipsData source;
    populate(&source, 0);
    Ship *const ship = add_ship(&source, 0);

    switch (modes[mode])
    {
      case ShipMode_Attack:
        ship_set_attack(ship, NULL);
        break;

      case ShipMode_FollowShip:
        ship_set_follow(ship, NULL, (FinePoint3d){5, 7, 11});
        break;

      case ShipMode_FlightPath:
        ship_set_flightpath(ship, &*waypoint);
        break;
    }

    static unsigned char buffer[Capacity];
    size_t const length = write_data(&source, false, &buffer);

    for (size_t offset = length; offset > 0;)
    {
      --offset;
      unsigned char const original = buffer[offset];

      for (size_t value = 0; value < ARRAY_SIZE(values); ++value)
      {
        if (values[value] == original)
        {
          continue;
        }

        buffer[offset] = values[value];
        ShipsData loaded;
        ships_init(&loaded);
        SFError const err = read_data(&loaded, &loaded_paths, false, &buffer, length);

        if (SFError_fail(err))
        {
          size_t found = 0;

          while ((found < ARRAY_SIZE(expected)) && (expected[found] != err.type))
          {
            ++found;
          }

          assert(found < ARRAY_SIZE(expected));

          if (!seen[found])
          {
            seen[found] = true;
            --remaining;
          }
        }

        ships_destroy(&loaded);

        if (remaining == 0)
        {
          break;
        }
      }

      buffer[offset] = original;

      if (remaining == 0)
      {
        break;
      }
    }

    ships_destroy(&source);

    if (remaining == 0)
    {
      break;
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(seen); ++i)
  {
    assert(seen[i]);
  }

  paths_destroy(&loaded_paths);
  paths_destroy(&paths);
}

static void test18(void)
{
  /* Ship stream position */

  PathsData paths;
  paths_init(&paths);
  ShipsData source;
  populate(&source, 3);

  for (int padded = 0; padded < 2; ++padded)
  {
    static unsigned char buffer[Capacity];
    size_t const length = write_data(&source, padded != 0, &buffer);
    ShipsData loaded;
    ships_init(&loaded);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    SFError const err = padded ? ships_read_pad(&loaded, &reader) : ships_read(&loaded, &reader);

    assert(!SFError_fail(err));
    assert(reader_ftell(&reader) == (long)length);
    assert(!SFError_fail(ships_post_read(&loaded, &paths)));
    check_pattern(&loaded, 3);

    reader_destroy(&reader);
    ships_destroy(&loaded);
  }

  ships_destroy(&source);
  paths_destroy(&paths);
}

static void test19(void)
{
  /* Ship output failure */

  ShipsData ships;
  populate(&ships, 1);
  ships_pre_write(&ships);

  for (int padded = 0; padded < 2; ++padded)
  {
    unsigned char buffer[64];
    Writer writer;
    assert(writer_mem_init(&writer, buffer, 0));

    if (padded)
    {
      ships_write_pad(&ships, &writer);
    }
    else
    {
      ships_write(&ships, &writer);
    }

    assert(writer_ferror(&writer));

    writer_destroy(&writer);
  }

  ships_destroy(&ships);
}

#ifdef FORTIFY
static void test20(void)
{
  /* Ship allocation failure */

  ShipsData ships;
  populate(&ships, 1);
  ShipValues const values = pattern(1);
  _Optional Ship *extra = ship_from_index(&ships, 0);

  Fortify_SetNumAllocationsLimit(0);
  SFError const err = ships_add(&ships, values.coords, values.direction,
    values.type, values.behaviour, values.importance, values.flags, values.pilot, &extra);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);

  assert(err.type == SFErrorType_NoMem);
  assert(extra == NULL);
  check_pattern(&ships, 1);

  ships_destroy(&ships);
}

static void test21(void)
{
  /* Ship read allocation failure */

  PathsData paths;
  paths_init(&paths);
  ShipsData source;
  populate(&source, 3);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(&source, false, &buffer);
  bool succeeded = false;

  for (unsigned long limit = 0; limit < 8; ++limit)
  {
    ShipsData loaded;
    ships_init(&loaded);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = ships_read(&loaded, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(!SFError_fail(ships_post_read(&loaded, &paths)));
      check_pattern(&loaded, 3);
      succeeded = true;
    }

    reader_destroy(&reader);
    ships_destroy(&loaded);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);

  ships_destroy(&source);
  paths_destroy(&paths);
}

#endif

void Ships_tests(void)
{
  /* Ships */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Ship flags round trip", test1 },
    { "Initial ship count", test2 },
    { "Added ship fields", test3 },
    { "Ship capacity", test4 },
    { "Ship addition without result", test5 },
    { "Ship deletion preserves survivors", test6 },
    { "Ship indices after deletion", test7 },
    { "Ship attack setter", test8 },
    { "Ship attack player", test9 },
    { "Ship follow setter", test10 },
    { "Ship follow player", test11 },
    { "Ship flightpath setter", test12 },
    { "Ship round trip", test13 },
    { "Ship relationship round trip", test14 },
    { "Ship follow player round trip", test15 },
    { "Ship truncation", test16 },
    { "Corrupt ship data", test17 },
    { "Ship stream position", test18 },
    { "Ship output failure", test19 },
#ifdef FORTIFY
    { "Ship allocation failure", test20 },
    { "Ship read allocation failure", test21 },
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
