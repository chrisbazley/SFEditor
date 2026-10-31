/* SFEditor test: ground map data.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "../DFile.h"
#include "../SFError.h"
#include "../Map.h"

/* Leave one quarter of the cells untouched. Different multipliers on each
 * axis help detect transposed data. */
static bool patterned_cell(MapCoord x, MapCoord y)
{
  return (x + 3 * y) % 4 != 0;
}

static unsigned char pattern_value(MapCoord x, MapCoord y, unsigned offset)
{
  return (unsigned char)(((unsigned)x + 5 * (unsigned)y + offset) % (Map_RefMax + 1));
}

static void check_pattern(MapData const *grid, bool overlay, unsigned offset)
{
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      unsigned char const expected = patterned_cell(x, y) ? pattern_value(x, y, offset) :
                                    (overlay ? Map_RefMask : 0);
      unsigned char const actual = map_ref_to_num(map_get_tile(grid, (MapPoint){x, y}));
      assert(actual == expected);
    }
  }
}

static void read_grid(DFile *file, unsigned char C23_CONST (*data)[Map_Area])
{
  Reader reader;
  assert(reader_mem_init(&reader, *data, sizeof(*data)));
  assert(!SFError_fail(dfile_read(file, &reader)));
  assert(!reader_ferror(&reader));
  reader_destroy(&reader);
}

/* Constructors allocate storage; initialise it through the reader. */
static MapData *make_grid(bool overlay)
{
  static unsigned char input[Map_Area];
  memset(input, overlay ? Map_RefMask : 0, sizeof(input));
  _Optional MapData *const grid = overlay ? map_create_overlay() : map_create_base();
  assert(grid);
  read_grid(map_get_dfile(&*grid), &input);
  return &*grid;
}

static void set_pattern(MapData *grid, bool wrapped)
{
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        MapPoint const p = wrapped ? (MapPoint){x - Map_Size, y + Map_Size} : (MapPoint){x, y};
        map_set_tile(grid, p, map_ref_from_num(pattern_value(x, y, 0)));
      }
    }
  }
}

static void check_set(bool overlay, bool wrapped)
{
  MapData *const grid = make_grid(overlay);
  set_pattern(grid, wrapped);
  check_pattern(grid, overlay, 0);
  dfile_release(map_get_dfile(grid));
}

static void check_round_trip(bool overlay)
{
  static unsigned char saved[Map_Area];
  MapData *const grid = make_grid(overlay);
  set_pattern(grid, false);
  check_pattern(grid, overlay, 0);

  Writer writer;
  assert(writer_mem_init(&writer, saved, sizeof(saved)));
  dfile_write(map_get_dfile(grid), &writer);
  assert(!writer_ferror(&writer));
  assert(writer_destroy(&writer) == Map_Area);
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      unsigned char const expected = patterned_cell(x, y) ? pattern_value(x, y, 0) :
                                    (overlay ? Map_RefMask : 0);
      assert(saved[(size_t)y * Map_Size + (size_t)x] == expected);
    }
  }
  check_pattern(grid, overlay, 0);

  _Optional MapData *const reloaded = overlay ? map_create_overlay() : map_create_base();
  assert(reloaded);
  read_grid(map_get_dfile(&*reloaded), &saved);
  check_pattern(&*reloaded, overlay, 0);
  dfile_release(map_get_dfile(&*reloaded));
  dfile_release(map_get_dfile(grid));
}

static void test1(void)
{
  /* Set base cells */

  check_set(false, false);
}

static void test2(void)
{
  /* Set overlay cells */

  check_set(true, false);
}

static void test3(void)
{
  /* Wrapped cell assignment */

  check_set(false, true);
}

static void test4(void)
{
  /* Update previous value */

  MapData *const grid = make_grid(false);
  set_pattern(grid, false);
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        MapRef const old = map_update_tile(grid, (MapPoint){x, y},
          map_ref_from_num(pattern_value(x, y, 1)));
        assert(map_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  check_pattern(grid, false, 1);
  dfile_release(map_get_dfile(grid));
}

static void test5(void)
{
  /* Unchanged update */

  MapData *const grid = make_grid(false);
  set_pattern(grid, false);
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        MapRef const old = map_update_tile(grid, (MapPoint){x, y},
          map_ref_from_num(pattern_value(x, y, 0)));
        assert(map_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  check_pattern(grid, false, 0);
  dfile_release(map_get_dfile(grid));
}

static void test6(void)
{
  /* Restore overlay mask */

  MapData *const grid = make_grid(true);
  set_pattern(grid, false);
  check_pattern(grid, true, 0);
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        MapRef const old = map_update_tile(grid, (MapPoint){x, y}, map_ref_from_num(Map_RefMask));
        assert(map_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      assert(map_ref_to_num(map_get_tile(grid, (MapPoint){x, y})) == Map_RefMask);
    }
  }
  dfile_release(map_get_dfile(grid));
}

static void test7(void)
{
  /* Base round trip */

  check_round_trip(false);
}

static void test8(void)
{
  /* Overlay round trip */

  check_round_trip(true);
}

void Map_tests(void)
{
  /* Map */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Set base cells", test1 },
    { "Set overlay cells", test2 },
    { "Wrapped cell assignment", test3 },
    { "Update previous value", test4 },
    { "Unchanged update", test5 },
    { "Restore overlay mask", test6 },
    { "Base round trip", test7 },
    { "Overlay round trip", test8 },
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);
    fflush(stdout);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
