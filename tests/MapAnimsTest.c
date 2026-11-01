/* SFEditor test: map animations.
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
#include "scheduler.h"
#include "../MapCoord.h"
#include "../MapAnims.h"
#include "../Map.h"
#include "../DFile.h"
#include "../MapAreaCol.h"
#include "../MapAreaColData.h"

/* Space for the small round-trip fixtures, kept off the stack. */
enum { Capacity = 1024 };

static MapPoint position(unsigned index)
{
  return (MapPoint){3 + ((index % 16) * 5), 7 + ((index / 16) * 11)};
}

static MapArea whole_map(void)
{
  return (MapArea){.min = {0, 0}, .max = {Map_Size - 1, Map_Size - 1}};
}

static MapAnimParam parameter(unsigned index)
{
  static uint16_t const periods[] = {0, 1, 2, 3, 17, UINT16_MAX};
  static unsigned char const tiles[] = {0, 1, 7, 31, 63, 127, Map_RefMax, Map_RefMask};
  MapAnimParam param = {.period = periods[index % ARRAY_SIZE(periods)]};

  for (size_t frame = 0; frame < AnimsNFrames; ++frame)
  {
    param.tiles[frame] = map_ref_from_num(tiles[(index + frame) % ARRAY_SIZE(tiles)]);
  }

  return param;
}

static MapAnimParam frames(uint16_t period)
{
  return (MapAnimParam){
    .period = period,
    .tiles = {map_ref_from_num(3), map_ref_from_num(5),
              map_ref_from_num(7), map_ref_from_num(11)},
  };
}

static ConvAnimations *populate(unsigned count)
{
  _Optional ConvAnimations *const anims = MapAnims_create();
  assert(anims);

  for (unsigned i = 0; i < count; ++i)
  {
    assert(!SFError_fail(MapAnims_add(&*anims, NULL, position(i), parameter(i))));
  }

  return &*anims;
}

static void destroy(ConvAnimations *anims)
{
  dfile_release(MapAnims_get_dfile(anims));
}

static void check_point(MapPoint actual, MapPoint expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
}

static void check_param(MapAnimParam actual, MapAnimParam expected)
{
  assert(actual.period == expected.period);

  for (size_t frame = 0; frame < AnimsNFrames; ++frame)
  {
    assert(map_ref_is_equal(actual.tiles[frame], expected.tiles[frame]));
  }
}

static void check_pattern(ConvAnimations *anims, unsigned count)
{
  assert(MapAnims_count(anims) == count);

  for (unsigned i = 0; i < count; ++i)
  {
    MapAnimParam actual;
    assert(MapAnims_get(anims, position(i), &actual));
    check_param(actual, parameter(i));
  }
}

static MapRef current_tile(ConvAnimations *anims, MapPoint pos)
{
  MapArea const area = {.min = pos, .max = pos};
  MapAnimsIter iter;
  check_point(MapAnimsIter_get_first(&iter, anims, &area, NULL), pos);
  assert(!MapAnimsIter_done(&iter));
  return MapAnimsIter_get_current(&iter);
}

static MapData *make_map(MapPoint pos)
{
  _Optional MapData *const map = map_create_base();
  assert(map);
  map_set_tile(&*map, pos, map_ref_from_num(99));
  map_set_tile(&*map, (MapPoint){pos.x + 1, pos.y}, map_ref_from_num(88));
  map_set_tile(&*map, (MapPoint){pos.x, pos.y + 1}, map_ref_from_num(77));
  return &*map;
}

static void check_neighbours(MapData const *map, MapPoint pos)
{
  assert(map_ref_to_num(map_get_tile(map, (MapPoint){pos.x + 1, pos.y})) == 88);
  assert(map_ref_to_num(map_get_tile(map, (MapPoint){pos.x, pos.y + 1})) == 77);
}

static size_t write_data(ConvAnimations *anims, unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  dfile_write(MapAnims_get_dfile(anims), &writer);
  assert(!writer_ferror(&writer));

  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(ConvAnimations *anims,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = dfile_read(MapAnims_get_dfile(anims), &reader);
  reader_destroy(&reader);
  return err;
}

static void test1(void)
{
  /* Initial animation count */

  ConvAnimations *const anims = populate(0);
  MapArea const area = whole_map();
  MapAnimsIter iter;

  MapAnimsIter_get_first(&iter, anims, &area, NULL);

  assert(MapAnims_count(anims) == 0);
  assert(MapAnimsIter_done(&iter));

  destroy(anims);
}

static void test2(void)
{
  /* Initial animation locations */

  ConvAnimations *const anims = populate(0);

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      assert(!MapAnims_check_locn(anims, (MapPoint){x, y}));
    }
  }

  destroy(anims);
}

static void test3(void)
{
  /* Added animation parameters */

  ConvAnimations *const anims = populate(AnimsMax);

  check_pattern(anims, AnimsMax);

  destroy(anims);
}

static void test4(void)
{
  /* Untouched animation locations */

  ConvAnimations *const anims = populate(16);

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      bool const expected = (y == 7) && (x >= 3) && (x < 83) && (((x - 3) % 5) == 0);
      assert(MapAnims_check_locn(anims, (MapPoint){x, y}) == expected);
    }
  }

  destroy(anims);
}

static void test5(void)
{
  /* Animation capacity */

  ConvAnimations *const anims = populate(AnimsMax);

  SFError const err = MapAnims_add(anims, NULL, (MapPoint){255, 255}, parameter(0));

  assert(err.type == SFErrorType_NumAnims);
  assert(!MapAnims_check_locn(anims, (MapPoint){255, 255}));
  check_pattern(anims, AnimsMax);

  destroy(anims);
}

static void test6(void)
{
  /* Animation coordinate wrapping */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const expected = parameter(3);

  assert(!SFError_fail(MapAnims_add(anims, NULL, (MapPoint){-1, Map_Size + 3}, expected)));

  MapAnimParam actual;
  assert(MapAnims_get(anims, (MapPoint){Map_Size - 1, 3}, &actual));
  check_param(actual, expected);
  assert(MapAnims_get(anims, (MapPoint){-1, Map_Size + 3}, &actual));
  check_param(actual, expected);

  destroy(anims);
}

static void test7(void)
{
  /* Spatial animation iteration */

  ConvAnimations *const anims = populate(48);
  MapArea const area = {.min = {8, 7}, .max = {8, 29}};
  MapAnimsIter iter;
  MapAnimParam actual;
  unsigned count = 0;

  for (MapPoint pos = MapAnimsIter_get_first(&iter, anims, &area, &actual);
       !MapAnimsIter_done(&iter); pos = MapAnimsIter_get_next(&iter, &actual))
  {
    unsigned const index = 1 + (16 * count);
    assert(count < 3);
    check_point(pos, position(index));
    check_param(actual, parameter(index));
    ++count;
  }

  assert(count == 3);

  destroy(anims);
}

static void test8(void)
{
  /* Animation iteration without parameters */

  ConvAnimations *const anims = populate(16);
  MapArea const area = whole_map();
  MapAnimsIter iter;
  unsigned count = 0;

  for (MapPoint pos = MapAnimsIter_get_first(&iter, anims, &area, NULL);
       !MapAnimsIter_done(&iter); pos = MapAnimsIter_get_next(&iter, NULL))
  {
    assert(count < 16);
    check_point(pos, position(count));
    ++count;
  }

  assert(count == 16);

  destroy(anims);
}

static void test9(void)
{
  /* Wrapped animation area */

  ConvAnimations *const anims = populate(0);
  static MapPoint const points[] = {
    {0, 0}, {Map_Size - 1, 0}, {0, Map_Size - 1}, {Map_Size - 1, Map_Size - 1},
    {1, 0}, {0, 1},
  };

  for (size_t i = 0; i < ARRAY_SIZE(points); ++i)
  {
    assert(!SFError_fail(MapAnims_add(anims, NULL, points[i], parameter(0))));
  }

  MapArea const area = {.min = {-1, -1}, .max = {0, 0}};
  MapAnimsIter iter;
  unsigned count = 0;

  for (MapPoint pos = MapAnimsIter_get_first(&iter, anims, &area, NULL);
       !MapAnimsIter_done(&iter); pos = MapAnimsIter_get_next(&iter, NULL))
  {
    assert(((pos.x + 1) % Map_Size) < 2);
    assert(((pos.y + 1) % Map_Size) < 2);
    ++count;
  }

  assert(count == 4);

  destroy(anims);
}

static void test10(void)
{
  /* Animation deletion during iteration */

  ConvAnimations *const anims = populate(16);
  MapArea const area = whole_map();
  MapAnimsIter iter;
  unsigned count = 0;

  for (MapPoint pos = MapAnimsIter_get_first(&iter, anims, &area, NULL);
       !MapAnimsIter_done(&iter); pos = MapAnimsIter_get_next(&iter, NULL))
  {
    check_point(pos, position(count));

    if ((count % 2) == 0)
    {
      MapAnimsIter_del_current(&iter);
    }

    ++count;
  }

  assert(count == 16);
  assert(MapAnims_count(anims) == 8);

  for (unsigned i = 0; i < 16; ++i)
  {
    MapAnimParam actual;
    bool const found = MapAnims_get(anims, position(i), &actual);
    assert(found == ((i % 2) != 0));
    assert(MapAnims_check_locn(anims, position(i)) == found);

    if (found)
    {
      check_param(actual, parameter(i));
    }
  }

  destroy(anims);
}

static void test11(void)
{
  /* Animation replacement preserves neighbours */

  ConvAnimations *const anims = populate(3);
  MapArea const area = {.min = {8, 7}, .max = {8, 7}};
  MapAnimsIter iter;
  MapAnimsIter_get_first(&iter, anims, &area, NULL);
  assert(!MapAnimsIter_done(&iter));

  MapAnimsIter_replace_current(&iter, parameter(99));

  assert(MapAnims_count(anims) == 3);

  for (unsigned i = 0; i < 3; ++i)
  {
    MapAnimParam actual;
    assert(MapAnims_get(anims, position(i), &actual));
    check_param(actual, parameter(i == 1 ? 99 : i));
  }

  destroy(anims);
}

static void test12(void)
{
  /* Initial animation frame */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(2))));

  assert(map_ref_to_num(current_tile(anims, position(0))) == 3);

  destroy(anims);
}

static void test13(void)
{
  /* Masked initial animation frame */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = {
    .period = 0,
    .tiles = {map_ref_mask(), map_ref_from_num(12), map_ref_mask(), map_ref_from_num(34)},
  };
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));

  assert(map_ref_to_num(current_tile(anims, position(0))) == 34);

  destroy(anims);
}

static void test14(void)
{
  /* Blank animation frame */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = {
    .period = 0,
    .tiles = {map_ref_mask(), map_ref_mask(), map_ref_mask(), map_ref_mask()},
  };
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));

  assert(map_ref_is_mask(current_tile(anims, position(0))));

  destroy(anims);
}

static void test15(void)
{
  /* Animation frame advance */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = frames(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));
  MapData *const map = make_map(position(0));

  for (unsigned step = 1; step <= 12; ++step)
  {
    assert(MapAnims_update(anims, map, 1, NULL) == 0);

    MapRef const expected = param.tiles[step % AnimsNFrames];
    assert(map_ref_is_equal(map_get_tile(map, position(0)), expected));
    assert(map_ref_is_equal(current_tile(anims, position(0)), expected));
    check_neighbours(map, position(0));
  }

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test16(void)
{
  /* Animation period countdown */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = frames(2);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));
  MapData *const map = make_map(position(0));

  for (int step = 0; step <= 12; ++step)
  {
    SchedulerTime const remaining = MapAnims_update(anims, map, step == 0 ? 0 : 1, NULL);

    assert(remaining == (2 - (step % 3)));
    assert(map_ref_is_equal(map_get_tile(map, position(0)), param.tiles[(step / 3) % AnimsNFrames]));
    check_neighbours(map, position(0));
  }

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test17(void)
{
  /* Masked animation frames preserve tile */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = {
    .period = 0,
    .tiles = {map_ref_from_num(3), map_ref_mask(), map_ref_from_num(7), map_ref_mask()},
  };
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));
  MapData *const map = make_map(position(0));
  static unsigned char const expected[] = {3, 3, 7, 7};

  for (size_t step = 0; step <= 8; ++step)
  {
    MapAnims_update(anims, map, step == 0 ? 0 : 1, NULL);

    assert(map_ref_to_num(map_get_tile(map, position(0))) == expected[step % ARRAY_SIZE(expected)]);
    assert(map_ref_to_num(current_tile(anims, position(0))) == expected[step % ARRAY_SIZE(expected)]);
    check_neighbours(map, position(0));
  }

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test18(void)
{
  /* Blank animation preserves map */

  ConvAnimations *const anims = populate(0);
  MapAnimParam const param = {
    .period = 0,
    .tiles = {map_ref_mask(), map_ref_mask(), map_ref_mask(), map_ref_mask()},
  };
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), param)));
  MapData *const map = make_map(position(0));

  MapAnims_update(anims, map, 7, NULL);

  assert(map_ref_to_num(map_get_tile(map, position(0))) == 99);
  check_neighbours(map, position(0));

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test19(void)
{
  /* Empty animation update */

  ConvAnimations *const anims = populate(0);
  MapData *const map = make_map(position(0));

  assert(MapAnims_update(anims, map, 7, NULL) == SchedulerTime_Max);

  assert(map_ref_to_num(map_get_tile(map, position(0))) == 99);
  check_neighbours(map, position(0));

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test20(void)
{
  /* Animation reset */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(1))));
  MapData *const map = make_map(position(0));
  MapAnims_update(anims, map, 5, NULL);
  assert(map_ref_to_num(current_tile(anims, position(0))) == 7);

  MapAnims_reset(anims);

  assert(map_ref_to_num(current_tile(anims, position(0))) == 3);
  assert(MapAnims_update(anims, map, 0, NULL) == 1);
  assert(map_ref_to_num(map_get_tile(map, position(0))) == 3);

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test21(void)
{
  /* Animation added after elapsed time */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(0))));
  MapData *const map = make_map(position(0));
  MapAnims_update(anims, map, 5, NULL);

  assert(!SFError_fail(MapAnims_add(anims, NULL, position(1), frames(1))));

  assert(map_ref_to_num(current_tile(anims, position(1))) == 7);

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test22(void)
{
  /* Animation redraw for changed tile */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(0))));
  MapData *const map = make_map(position(0));
  MapAreaColData redraw;
  MapAreaCol_init(&redraw, Map_SizeLog2);

  MapAnims_update(anims, map, 0, &redraw);

  MapAreaColIter iter;
  _Optional MapArea const *const area = MapAreaColIter_get_first(&iter, &redraw);
  assert(area);
  check_point(area->min, position(0));
  check_point(area->max, position(0));
  assert(MapAreaColIter_get_next(&iter) == NULL);

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test23(void)
{
  /* Unchanged animation tile avoids redraw */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(0))));
  MapData *const map = make_map(position(0));
  map_set_tile(map, position(0), map_ref_from_num(3));
  MapAreaColData redraw;
  MapAreaCol_init(&redraw, Map_SizeLog2);

  MapAnims_update(anims, map, 0, &redraw);

  MapAreaColIter iter;
  assert(MapAreaColIter_get_first(&iter, &redraw) == NULL);

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test24(void)
{
  /* Earliest animation update */

  ConvAnimations *const anims = populate(0);
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(0), frames(1))));
  assert(!SFError_fail(MapAnims_add(anims, NULL, position(1), frames(3))));
  MapData *const map = make_map(position(0));
  map_set_tile(map, position(1), map_ref_from_num(99));

  assert(MapAnims_update(anims, map, 0, NULL) == 1);
  assert(MapAnims_update(anims, map, 1, NULL) == 0);
  assert(MapAnims_update(anims, map, 1, NULL) == 1);
  assert(MapAnims_update(anims, map, 2, NULL) == 1);

  dfile_release(map_get_dfile(map));
  destroy(anims);
}

static void test25(void)
{
  /* Animation round trip */

  for (unsigned count = 0; count <= 16; ++count)
  {
    ConvAnimations *const source = populate(count);
    static unsigned char buffer[Capacity];
    size_t const length = write_data(source, &buffer);
    ConvAnimations *const loaded = populate(0);

    assert(!SFError_fail(read_data(loaded, &buffer, length)));

    check_pattern(loaded, count);
    assert(dfile_get_min_size(MapAnims_get_dfile(loaded)) == (long)length);

    destroy(loaded);
    destroy(source);
  }
}

static void test26(void)
{
  /* Animation reload replaces data */

  ConvAnimations *const source = populate(3);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  ConvAnimations *const loaded = populate(5);

  assert(!SFError_fail(read_data(loaded, &buffer, length)));

  check_pattern(loaded, 3);
  assert(!MapAnims_check_locn(loaded, position(3)));
  assert(!MapAnims_check_locn(loaded, position(4)));

  destroy(loaded);
  destroy(source);
}

static void test27(void)
{
  /* Animation truncation */

  ConvAnimations *const source = populate(3);
  static unsigned char buffer[Capacity];
  size_t const size = write_data(source, &buffer);

  for (size_t length = 0; length < size; ++length)
  {
    ConvAnimations *const loaded = populate(0);

    assert(SFError_fail(read_data(loaded, &buffer, length)));

    destroy(loaded);
  }

  destroy(source);
}

static void test28(void)
{
  /* Animation extension */

  ConvAnimations *const source = populate(3);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  ConvAnimations *const loaded = populate(0);

  assert(read_data(loaded, &buffer, length + 1).type == SFErrorType_TooLong);

  destroy(loaded);
  destroy(source);
}

static void test29(void)
{
  /* Corrupt animation data */

  ConvAnimations *const source = populate(2);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  static SFErrorType const expected[] = {
    SFErrorType_BadNumAnims, SFErrorType_BadAnimCoord, SFErrorType_AnimOverlap,
    SFErrorType_BadAnimTime, SFErrorType_BadAnimState, SFErrorType_BadAnimFrame,
    SFErrorType_Trunc, SFErrorType_TooLong,
  };
  bool seen[ARRAY_SIZE(expected)] = {false};
  size_t remaining = ARRAY_SIZE(expected);

  /* Chosen by comparing loader branch coverage with the exhaustive sweep. */
  static unsigned char const values[] = {0, 5, 32, UCHAR_MAX};

  ConvAnimations *const loaded = populate(0);

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
      SFError const err = read_data(loaded, &buffer, length);

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

  for (size_t i = 0; i < ARRAY_SIZE(seen); ++i)
  {
    assert(seen[i]);
  }

  destroy(loaded);
  destroy(source);
}

static void test30(void)
{
  /* Animation output failure */

  ConvAnimations *const anims = populate(1);
  unsigned char buffer[64];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  dfile_write(MapAnims_get_dfile(anims), &writer);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  destroy(anims);
}

#ifdef FORTIFY
static void test31(void)
{
  /* Animation creation allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    _Optional ConvAnimations *const anims = MapAnims_create();
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (anims)
    {
      assert(limit > 0);
      check_pattern(&*anims, 0);
      destroy(&*anims);
      succeeded = true;
      break;
    }
  }

  assert(succeeded);
}

static void test32(void)
{
  /* Animation addition allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    ConvAnimations *const anims = populate(1);

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = MapAnims_add(anims, NULL, position(1), parameter(1));
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      check_pattern(anims, 1);
      assert(!MapAnims_check_locn(anims, position(1)));
    }
    else
    {
      check_pattern(anims, 2);
      succeeded = true;
    }

    destroy(anims);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);
}

static void test33(void)
{
  /* Animation read allocation failure */

  ConvAnimations *const source = populate(3);
  static unsigned char buffer[Capacity];
  size_t const length = write_data(source, &buffer);
  bool succeeded = false;

  for (unsigned long limit = 0; limit < 32; ++limit)
  {
    ConvAnimations *const loaded = populate(0);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = dfile_read(MapAnims_get_dfile(loaded), &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      check_pattern(loaded, 3);
      succeeded = true;
    }

    reader_destroy(&reader);
    destroy(loaded);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);

  destroy(source);
}

#endif

void MapAnims_tests(void)
{
  /* MapAnims */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial animation count", test1 },
    { "Initial animation locations", test2 },
    { "Added animation parameters", test3 },
    { "Untouched animation locations", test4 },
    { "Animation capacity", test5 },
    { "Animation coordinate wrapping", test6 },
    { "Spatial animation iteration", test7 },
    { "Animation iteration without parameters", test8 },
    { "Wrapped animation area", test9 },
    { "Animation deletion during iteration", test10 },
    { "Animation replacement preserves neighbours", test11 },
    { "Initial animation frame", test12 },
    { "Masked initial animation frame", test13 },
    { "Blank animation frame", test14 },
    { "Animation frame advance", test15 },
    { "Animation period countdown", test16 },
    { "Masked animation frames preserve tile", test17 },
    { "Blank animation preserves map", test18 },
    { "Empty animation update", test19 },
    { "Animation reset", test20 },
    { "Animation added after elapsed time", test21 },
    { "Animation redraw for changed tile", test22 },
    { "Unchanged animation tile avoids redraw", test23 },
    { "Earliest animation update", test24 },
    { "Animation round trip", test25 },
    { "Animation reload replaces data", test26 },
    { "Animation truncation", test27 },
    { "Animation extension", test28 },
    { "Corrupt animation data", test29 },
    { "Animation output failure", test30 },
#ifdef FORTIFY
    { "Animation creation allocation failure", test31 },
    { "Animation addition allocation failure", test32 },
    { "Animation read allocation failure", test33 },
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
