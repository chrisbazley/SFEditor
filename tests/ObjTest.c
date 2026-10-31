/* SFEditor test: objects grid data.
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
#include "../Obj.h"

/* Leave one quarter of the cells untouched. Different multipliers on each
 * axis help detect transposed data. */
static bool patterned_cell(MapCoord x, MapCoord y)
{
  if (y == 0 || y == Obj_Size - 2)
  {
    return false;
  }
  return (x + 3 * y) % 4 != 0;
}

static unsigned char pattern_value(MapCoord x, MapCoord y, unsigned offset)
{
  return (unsigned char)(((unsigned)x + 5 * (unsigned)y + offset) % (Obj_RefHill + 1));
}

static void check_pattern(ObjectsData const *grid, bool overlay, unsigned offset)
{
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      unsigned char const expected = patterned_cell(x, y) ? pattern_value(x, y, offset) :
                                    (overlay ? Obj_RefMask : 0);
      unsigned char const actual = objects_ref_to_num(objects_get_ref(grid, (MapPoint){x, y}));
      assert(actual == expected);
    }
  }
}

static void read_grid(DFile *file, unsigned char C23_CONST (*data)[Obj_Area])
{
  Reader reader;
  assert(reader_mem_init(&reader, *data, sizeof(*data)));
  assert(!SFError_fail(dfile_read(file, &reader)));
  assert(!reader_ferror(&reader));
  reader_destroy(&reader);
}

/* Constructors allocate storage; initialise it through the reader. */
static ObjectsData *make_grid(bool overlay)
{
  static unsigned char input[Obj_Area];
  memset(input, overlay ? Obj_RefMask : 0, sizeof(input));
  _Optional ObjectsData *const grid = overlay ? objects_create_overlay() : objects_create_base();
  assert(grid);
  read_grid(objects_get_dfile(&*grid), &input);
  return &*grid;
}

static void set_pattern(ObjectsData *grid, bool wrapped)
{
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        MapPoint const p = wrapped ? (MapPoint){x - Obj_Size, y + Obj_Size} : (MapPoint){x, y};
        objects_set_ref(grid, p, objects_ref_from_num(pattern_value(x, y, 0)));
      }
    }
  }
}

static void check_set(bool overlay, bool wrapped)
{
  ObjectsData *const grid = make_grid(overlay);
  set_pattern(grid, wrapped);
  check_pattern(grid, overlay, 0);
  dfile_release(objects_get_dfile(grid));
}

static void check_round_trip(bool overlay)
{
  static unsigned char saved[Obj_Area];
  ObjectsData *const grid = make_grid(overlay);
  set_pattern(grid, false);
  check_pattern(grid, overlay, 0);

  Writer writer;
  assert(writer_mem_init(&writer, saved, sizeof(saved)));
  dfile_write(objects_get_dfile(grid), &writer);
  assert(!writer_ferror(&writer));
  assert(writer_destroy(&writer) == Obj_Area);
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      unsigned char const expected = patterned_cell(x, y) ? pattern_value(x, y, 0) :
                                    (overlay ? Obj_RefMask : 0);
      assert(saved[(size_t)y * Obj_Size + (size_t)x] == expected);
    }
  }
  check_pattern(grid, overlay, 0);

  _Optional ObjectsData *const reloaded = overlay ? objects_create_overlay() : objects_create_base();
  assert(reloaded);
  read_grid(objects_get_dfile(&*reloaded), &saved);
  check_pattern(&*reloaded, overlay, 0);
  dfile_release(objects_get_dfile(&*reloaded));
  dfile_release(objects_get_dfile(grid));
}

static void check_forbidden_rows(bool overlay)
{
  static unsigned char input[Obj_Area];
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      /* Include objects in the forbidden rows in the input. */
      input[(size_t)y * Obj_Size + (size_t)x] = (x + 3 * y) % 4 != 0 ?
        pattern_value(x, y, 0) : (overlay ? Obj_RefMask : 0);
    }
  }
  _Optional ObjectsData *const grid = overlay ? objects_create_overlay() : objects_create_base();
  assert(grid);
  read_grid(objects_get_dfile(&*grid), &input);
  check_pattern(&*grid, overlay, 0);
  dfile_release(objects_get_dfile(&*grid));
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

  ObjectsData *const grid = make_grid(false);
  set_pattern(grid, false);
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        ObjRef const old = objects_update_ref(grid, (MapPoint){x, y},
          objects_ref_from_num(pattern_value(x, y, 1)));
        assert(objects_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  check_pattern(grid, false, 1);
  dfile_release(objects_get_dfile(grid));
}

static void test5(void)
{
  /* Unchanged update */

  ObjectsData *const grid = make_grid(false);
  set_pattern(grid, false);
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        ObjRef const old = objects_update_ref(grid, (MapPoint){x, y},
          objects_ref_from_num(pattern_value(x, y, 0)));
        assert(objects_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  check_pattern(grid, false, 0);
  dfile_release(objects_get_dfile(grid));
}

static void test6(void)
{
  /* Restore overlay mask */

  ObjectsData *const grid = make_grid(true);
  set_pattern(grid, false);
  check_pattern(grid, true, 0);
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      if (patterned_cell(x, y))
      {
        ObjRef const old = objects_update_ref(grid, (MapPoint){x, y}, objects_ref_from_num(Obj_RefMask));
        assert(objects_ref_to_num(old) == pattern_value(x, y, 0));
      }
    }
  }
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      assert(objects_ref_to_num(objects_get_ref(grid, (MapPoint){x, y})) == Obj_RefMask);
    }
  }
  dfile_release(objects_get_dfile(grid));
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

static void test9(void)
{
  /* Base forbidden rows */

  check_forbidden_rows(false);
}

static void test10(void)
{
  /* Overlay forbidden rows */

  check_forbidden_rows(true);
}

void Obj_tests(void)
{
  /* Obj */
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
    { "Base forbidden rows", test9 },
    { "Overlay forbidden rows", test10 },
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
