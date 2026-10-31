/* SFEditor test: mission flightpaths.
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
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../Paths.h"
#include "../PathsData.h"

/* Padded path data needs more space than the small models. Keep it off
 * the stack; the capacity is independent of the encoded record layout. */
enum { Capacity = 4096, SearchLimit = 256 };
static unsigned char data[Capacity];

static CoarsePoint3d pattern(unsigned path, unsigned waypoint)
{
  return (CoarsePoint3d){
    .x = (CoarseCoord)(path * 37 + waypoint * 17),
    .y = (CoarseCoord)(255 - path * 11 - waypoint * 23),
    .z = (CoarseCoord)(path * 53 + waypoint * 31),
  };
}

static void populate(PathsData *paths, int count, int waypoints)
{
  paths_init(paths);
  for (int i = 0; i < count; ++i)
  {
    _Optional Path *const path = paths_add(paths);
    assert(path);
    for (int j = 0; j < waypoints; ++j)
    {
      assert(path_add_waypoint(&*path, pattern((unsigned)i, (unsigned)j)));
    }
  }
}

static void check_topology(PathsData *paths, int count, int waypoints)
{
  for (int i = 0; i < count; ++i)
  {
    _Optional Path *const path = path_from_index(paths, i);
    assert(path);
    for (int j = 0; j < waypoints; ++j)
    {
      _Optional Waypoint *const waypoint = waypoint_from_index(&*path, j);
      assert(waypoint);
      assert(waypoint_get_path(&*waypoint) == path);
      if (j > 0)
      {
        assert(waypoint != waypoint_from_index(&*path, j - 1));
      }
    }
    /* Index 1 intentionally falls back to index 0 for a one-point path. */
    int const missing = waypoints == 1 ? 2 : waypoints;
    assert(waypoint_from_index(&*path, missing) == NULL);
  }
  assert(path_from_index(paths, count) == NULL);
}

static void check_point(Waypoint const *waypoint, CoarsePoint3d expected)
{
  CoarsePoint3d const actual = waypoint_get_coords(waypoint);
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
  assert(actual.z == expected.z);
}

static void check_coords(PathsData *paths, int count, int waypoints)
{
  for (int i = 0; i < count; ++i)
  {
    _Optional Path *const path = path_from_index(paths, i);
    assert(path);
    for (int j = 0; j < waypoints; ++j)
    {
      _Optional Waypoint *const waypoint = waypoint_from_index(&*path, j);
      assert(waypoint);
      check_point(&*waypoint, pattern((unsigned)i, (unsigned)j));
    }
  }
}

static size_t write_data(PathsData *paths, bool padded,
  unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));
  paths_pre_write(paths);
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  if (padded) { paths_write_pad(paths, &writer); }
  else { paths_write(paths, &writer); }
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise trailing padding without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(PathsData *paths, bool padded,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  Reader reader;
  assert(length <= sizeof(*buffer));
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = padded ? paths_read_pad(paths, &reader) : paths_read(paths, &reader);
  reader_destroy(&reader);
  return err;
}

static void check_round_trip(PathsData *source, bool padded,
  int count, int waypoints)
{
  size_t const length = write_data(source, padded, &data);
  PathsData loaded;
  paths_init(&loaded);
  assert(!SFError_fail(read_data(&loaded, padded, &data, length)));
  check_topology(&loaded, count, waypoints);
  check_coords(&loaded, count, waypoints);
  paths_destroy(&loaded);
}

static void test1(void)
{
  /* Initially empty paths */

  PathsData paths;
  paths_init(&paths);
  assert(path_from_index(&paths, 0) == NULL);
  paths_destroy(&paths);
}

static void test2(void)
{
  /* Added path order */

  PathsData paths;
  paths_init(&paths);
  _Optional Path *const first = paths_add(&paths);
  assert(first);
  _Optional Path *const second = paths_add(&paths);
  assert(second);
  assert(first != second);
  assert(path_from_index(&paths, 0) == first);
  assert(path_from_index(&paths, 1) == second);
  assert(path_from_index(&paths, 2) == NULL);
  paths_destroy(&paths);
}

static void test3(void)
{
  /* Initially empty waypoints */

  PathsData paths;
  populate(&paths, 1, 0);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  assert(waypoint_from_index(&*path, 0) == NULL);
  assert(waypoint_from_index(&*path, 1) == NULL);
  paths_destroy(&paths);
}

static void test4(void)
{
  /* Added waypoint order */

  PathsData paths;
  populate(&paths, 3, 4);
  check_topology(&paths, 3, 4);
  paths_destroy(&paths);
}

static void test5(void)
{
  /* Invalid path index */

  PathsData paths;
  populate(&paths, 1, 1);
  assert(path_from_index(&paths, -1) == NULL);
  assert(path_from_index(&paths, INT_MAX) == NULL);
  paths_destroy(&paths);
}

static void test6(void)
{
  /* Invalid waypoint index */

  PathsData paths;
  populate(&paths, 1, 2);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  assert(waypoint_from_index(&*path, -1) == NULL);
  assert(waypoint_from_index(&*path, INT_MAX) == NULL);
  paths_destroy(&paths);
}

static void test7(void)
{
  /* Legacy starting waypoint fallback */

  PathsData paths;
  populate(&paths, 1, 1);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  assert(waypoint_from_index(&*path, 1) == waypoint_from_index(&*path, 0));
  paths_destroy(&paths);
}

static void test8(void)
{
  /* Written path indices */

  PathsData paths;
  populate(&paths, 3, 0);
  _Optional Path *const first = path_from_index(&paths, 0);
  _Optional Path *const second = path_from_index(&paths, 1);
  _Optional Path *const third = path_from_index(&paths, 2);
  assert(first);
  assert(second);
  assert(third);
  paths_pre_write(&paths);
  assert(path_get_index(&*first) == 0);
  assert(path_get_index(&*second) == 1);
  assert(path_get_index(&*third) == 2);
  paths_destroy(&paths);
}

static void test9(void)
{
  /* Written waypoint indices */

  PathsData paths;
  populate(&paths, 1, 3);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  _Optional Waypoint *const first = waypoint_from_index(&*path, 0);
  _Optional Waypoint *const second = waypoint_from_index(&*path, 1);
  _Optional Waypoint *const third = waypoint_from_index(&*path, 2);
  assert(first);
  assert(second);
  assert(third);
  paths_pre_write(&paths);
  assert(waypoint_get_index(&*first) == 0);
  assert(waypoint_get_index(&*second) == 1);
  assert(waypoint_get_index(&*third) == 2);
  paths_destroy(&paths);
}

static void test10(void)
{
  /* Path deletion */

  for (int index = 0; index < 3; ++index)
  {
    PathsData paths;
    populate(&paths, 3, 2);
    _Optional Path *const first = path_from_index(&paths, 0);
    _Optional Path *const second = path_from_index(&paths, 1);
    _Optional Path *const third = path_from_index(&paths, 2);
    _Optional Path *const removed = path_from_index(&paths, index);
    assert(removed);
    paths_pre_write(&paths);
    path_delete(&*removed);
    _Optional Path *const survivor1 = index == 0 ? second : first;
    _Optional Path *const survivor2 = index == 2 ? second : third;
    assert(survivor1);
    assert(survivor2);
    assert(path_from_index(&paths, 0) == survivor1);
    assert(path_from_index(&paths, 1) == survivor2);
    check_topology(&paths, 2, 2);
    for (int i = 0; i < 2; ++i)
    {
      _Optional Path *const path = path_from_index(&paths, i);
      assert(path);
      unsigned const original = (unsigned)(i < index ? i : i + 1);
      for (int j = 0; j < 2; ++j)
      {
        _Optional Waypoint *const waypoint = waypoint_from_index(&*path, j);
        assert(waypoint);
        check_point(&*waypoint, pattern(original, (unsigned)j));
      }
    }
    paths_pre_write(&paths);
    assert(path_get_index(&*survivor1) == 0);
    assert(path_get_index(&*survivor2) == 1);
    paths_destroy(&paths);
  }
}

static void test11(void)
{
  /* Waypoint deletion */

  for (int index = 0; index < 3; ++index)
  {
    PathsData paths;
    populate(&paths, 1, 3);
    _Optional Path *const path = path_from_index(&paths, 0);
    assert(path);
    _Optional Waypoint *const first = waypoint_from_index(&*path, 0);
    _Optional Waypoint *const second = waypoint_from_index(&*path, 1);
    _Optional Waypoint *const third = waypoint_from_index(&*path, 2);
    _Optional Waypoint *const removed = waypoint_from_index(&*path, index);
    assert(removed);
    paths_pre_write(&paths);
    waypoint_delete(&*removed);
    _Optional Waypoint *const survivor1 = index == 0 ? second : first;
    _Optional Waypoint *const survivor2 = index == 2 ? second : third;
    assert(survivor1);
    assert(survivor2);
    assert(waypoint_from_index(&*path, 0) == survivor1);
    assert(waypoint_from_index(&*path, 1) == survivor2);
    check_topology(&paths, 1, 2);
    for (int j = 0; j < 2; ++j)
    {
      _Optional Waypoint *const waypoint = waypoint_from_index(&*path, j);
      assert(waypoint);
      unsigned const original = (unsigned)(j < index ? j : j + 1);
      check_point(&*waypoint, pattern(0, original));
    }
    paths_pre_write(&paths);
    assert(waypoint_get_index(&*survivor1) == 0);
    assert(waypoint_get_index(&*survivor2) == 1);
    paths_destroy(&paths);
  }
}

static void test12(void)
{
  /* Path addition after indexing */

  PathsData paths;
  populate(&paths, 1, 0);
  paths_pre_write(&paths);
  _Optional Path *const added = paths_add(&paths);
  assert(added);
  assert(path_from_index(&paths, 1) == added);
  paths_pre_write(&paths);
  assert(path_get_index(&*added) == 1);
  paths_destroy(&paths);
}

static void test13(void)
{
  /* Waypoint addition after indexing */

  PathsData paths;
  populate(&paths, 1, 0);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  paths_pre_write(&paths);
  _Optional Waypoint *const added = path_add_waypoint(&*path, pattern(0, 0));
  assert(added);
  assert(waypoint_from_index(&*path, 0) == added);
  paths_pre_write(&paths);
  assert(waypoint_get_index(&*added) == 0);
  paths_destroy(&paths);
}

static void test14(void)
{
  /* Path capacity */

  PathsData paths;
  paths_init(&paths);
  int count = 0;
  while (paths_add(&paths))
  {
    ++count;
    assert(count < SearchLimit);
  }
  assert(count > 0);
  check_topology(&paths, count, 0);
  _Optional Path *const first = path_from_index(&paths, 0);
  assert(first);
  path_delete(&*first);
  _Optional Path *const added = paths_add(&paths);
  assert(added);
  assert(path_from_index(&paths, count - 1) == added);
  assert(paths_add(&paths) == NULL);
  paths_destroy(&paths);
}

static void test15(void)
{
  /* Waypoint capacity */

  PathsData paths;
  populate(&paths, 1, 0);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  int count = 0;
  while (path_add_waypoint(&*path, pattern(0, (unsigned)count)))
  {
    ++count;
    assert(count < SearchLimit);
  }
  assert(count > 1);
  check_topology(&paths, 1, count);
  _Optional Waypoint *const first = waypoint_from_index(&*path, 0);
  assert(first);
  waypoint_delete(&*first);
  assert(path_add_waypoint(&*path, pattern(0, (unsigned)count)));
  assert(path_add_waypoint(&*path, pattern(0, 0)) == NULL);
  paths_destroy(&paths);
}

static void test16(void)
{
  /* Path round trip */

  for (int count = 0; count <= 3; ++count)
  {
    for (int waypoints = 0; waypoints <= 4; ++waypoints)
    {
      PathsData paths;
      populate(&paths, count, waypoints);
      check_round_trip(&paths, false, count, waypoints);
      paths_destroy(&paths);
    }
  }
}

static void test17(void)
{
  /* Padded path round trip */

  for (int count = 0; count <= 3; ++count)
  {
    for (int waypoints = 0; waypoints <= 4; ++waypoints)
    {
      PathsData paths;
      populate(&paths, count, waypoints);
      check_round_trip(&paths, true, count, waypoints);
      paths_destroy(&paths);
    }
  }
}

static void test18(void)
{
  /* Full path round trip */

  PathsData paths;
  paths_init(&paths);
  int count = 0, waypoints = 0;
  _Optional Path *path;
  while ((path = paths_add(&paths)) != NULL)
  {
    waypoints = 0;
    while (path_add_waypoint(&*path, pattern((unsigned)count, (unsigned)waypoints)))
    {
      ++waypoints;
      assert(waypoints < SearchLimit);
    }
    ++count;
    assert(count < SearchLimit);
  }
  assert(count > 0);
  check_round_trip(&paths, false, count, waypoints);
  check_round_trip(&paths, true, count, waypoints);
  paths_destroy(&paths);
}

static void test19(void)
{
  /* Path truncation */

  for (int padded = 0; padded < 2; ++padded)
  {
    PathsData source;
    populate(&source, 2, 3);
    size_t const size = write_data(&source, padded != 0, &data);
    for (size_t length = 0; length < size; ++length)
    {
      PathsData loaded;
      paths_init(&loaded);
      SFError const err = read_data(&loaded, padded != 0, &data, length);
      if (length == 0) { assert(SFError_fail(err)); }
      if (!SFError_fail(err))
      {
        check_topology(&loaded, 2, 3);
        check_coords(&loaded, 2, 3);
      }
      paths_destroy(&loaded);
    }
    paths_destroy(&source);
  }
}

static void test20(void)
{
  /* Corrupt path data */

  PathsData source;
  populate(&source, 2, 3);
  size_t const length = write_data(&source, false, &data);
  bool bad_paths = false, bad_waypoints = false, read_fail = false;
  for (size_t offset = 0; offset < length; ++offset)
  {
    unsigned char const original = data[offset];
    for (unsigned byte = 0; byte <= UCHAR_MAX; ++byte)
    {
      data[offset] = (unsigned char)byte;
      PathsData loaded;
      paths_init(&loaded);
      SFError const err = read_data(&loaded, false, &data, length);
      switch (err.type)
      {
        case SFErrorType_OK: break;
        case SFErrorType_BadNumPaths: bad_paths = true; break;
        case SFErrorType_BadNumWaypoints: bad_waypoints = true; break;
        case SFErrorType_ReadFail: read_fail = true; break;
        default: assert(false); break;
      }
      paths_destroy(&loaded);
    }
    data[offset] = original;
  }
  assert(bad_paths);
  assert(bad_waypoints);
  assert(read_fail);
  paths_destroy(&source);
}

static void test21(void)
{
  /* Path output failure */

  for (int padded = 0; padded < 2; ++padded)
  {
    PathsData paths;
    populate(&paths, 2, 3);
    paths_pre_write(&paths);
    Writer writer;
    assert(writer_mem_init(&writer, data, 0));
    if (padded) { paths_write_pad(&paths, &writer); }
    else { paths_write(&paths, &writer); }
    assert(writer_ferror(&writer));
    writer_destroy(&writer);
    paths_destroy(&paths);
  }
}

#ifdef FORTIFY
static void test22(void)
{
  /* Path allocation failure */

  PathsData paths;
  populate(&paths, 1, 1);
  _Optional Path *const first = path_from_index(&paths, 0);
  assert(first);
  Fortify_SetNumAllocationsLimit(0);
  _Optional Path *const failed = paths_add(&paths);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(failed == NULL);
  assert(path_from_index(&paths, 0) == first);
  check_topology(&paths, 1, 1);
  assert(paths_add(&paths));
  paths_destroy(&paths);
}

#endif

#ifdef FORTIFY
static void test23(void)
{
  /* Waypoint allocation failure */

  PathsData paths;
  populate(&paths, 1, 1);
  _Optional Path *const path = path_from_index(&paths, 0);
  assert(path);
  _Optional Waypoint *const first = waypoint_from_index(&*path, 0);
  assert(first);
  Fortify_SetNumAllocationsLimit(0);
  _Optional Waypoint *const failed = path_add_waypoint(&*path, pattern(0, 1));
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(failed == NULL);
  assert(waypoint_from_index(&*path, 0) == first);
  check_topology(&paths, 1, 1);
  assert(path_add_waypoint(&*path, pattern(0, 1)));
  check_topology(&paths, 1, 2);
  paths_destroy(&paths);
}

#endif

#ifdef FORTIFY
static void test24(void)
{
  /* Path read allocation failure */

  PathsData source;
  populate(&source, 2, 3);
  size_t const length = write_data(&source, false, &data);
  bool succeeded = false;
  for (unsigned long limit = 0; limit < SearchLimit; ++limit)
  {
    Reader reader;
    assert(reader_mem_init(&reader, data, length));
    PathsData loaded;
    paths_init(&loaded);
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = paths_read(&loaded, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(limit > 0);
      check_topology(&loaded, 2, 3);
      succeeded = true;
    }
    paths_destroy(&loaded);
    reader_destroy(&reader);
    if (succeeded) { break; }
  }
  assert(succeeded);
  paths_destroy(&source);
}

#endif

static void test25(void)
{
  /* Path stream position */

  for (int padded = 0; padded < 2; ++padded)
  {
    PathsData source, loaded;
    populate(&source, 2, 3);
    size_t const length = write_data(&source, padded != 0, &data);
    Reader reader;
    assert(reader_mem_init(&reader, data, length));
    paths_init(&loaded);
    SFError const err = padded ? paths_read_pad(&loaded, &reader) : paths_read(&loaded, &reader);
    assert(!SFError_fail(err));
    assert(reader_ftell(&reader) == (long)length);
    reader_destroy(&reader);
    paths_destroy(&loaded);
    paths_destroy(&source);
  }
}

static void test26(void)
{
  /* Waypoint coordinates */

  for (unsigned value = 0; value <= UINT8_MAX; ++value)
  {
    PathsData paths;
    paths_init(&paths);
    _Optional Path *const path = paths_add(&paths);
    assert(path);
    CoarsePoint3d const coords = {
      .x = (CoarseCoord)value,
      .y = (CoarseCoord)(UINT8_MAX - value),
      .z = (CoarseCoord)(value * 17),
    };
    _Optional Waypoint *const waypoint = path_add_waypoint(&*path, coords);
    assert(waypoint);
    check_point(&*waypoint, coords);
    paths_destroy(&paths);
  }
}

void Paths_tests(void)
{
  /* Paths */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initially empty paths", test1 },
    { "Added path order", test2 },
    { "Initially empty waypoints", test3 },
    { "Added waypoint order", test4 },
    { "Waypoint coordinates", test26 },
    { "Invalid path index", test5 },
    { "Invalid waypoint index", test6 },
    { "Legacy starting waypoint fallback", test7 },
    { "Written path indices", test8 },
    { "Written waypoint indices", test9 },
    { "Path deletion", test10 },
    { "Waypoint deletion", test11 },
    { "Path addition after indexing", test12 },
    { "Waypoint addition after indexing", test13 },
    { "Path capacity", test14 },
    { "Waypoint capacity", test15 },
    { "Path round trip", test16 },
    { "Padded path round trip", test17 },
    { "Full path round trip", test18 },
    { "Path truncation", test19 },
    { "Corrupt path data", test20 },
    { "Path output failure", test21 },
    { "Path stream position", test25 },
#ifdef FORTIFY
    { "Path allocation failure", test22 },
#endif
#ifdef FORTIFY
    { "Waypoint allocation failure", test23 },
#endif
#ifdef FORTIFY
    { "Path read allocation failure", test24 },
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
