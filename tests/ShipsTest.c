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

static void check_flags(ShipFlags actual, ShipFlags expected)
{
  assert(actual.is_friendly == expected.is_friendly);
  assert(actual.is_cloaked == expected.is_cloaked);
  assert(actual.is_pacifist == expected.is_pacifist);
  assert(actual.is_alert == expected.is_alert);
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
